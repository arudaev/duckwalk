//! Bluetooth on Windows, through WinRT. btleplug finds only devices that
//! advertise, and a connected Harold doesn't; Windows lists the GATT service
//! of every paired device instead, which is how the C helper finds him too.

use tokio::sync::watch;
use windows::core::{Result, GUID};
use windows::Devices::Bluetooth::BluetoothCacheMode;
use windows::Devices::Bluetooth::GenericAttributeProfile::{
    GattCharacteristic, GattClientCharacteristicConfigurationDescriptorValue, GattCommunicationStatus,
    GattDeviceService, GattValueChangedEventArgs, GattWriteOption,
};
use windows::Devices::Enumeration::DeviceInformation;
use windows::Foundation::TypedEventHandler;
use windows::Storage::Streams::{DataReader, DataWriter};

use super::{linked, LOOK_AGAIN, NOTIFY, PACKET, SERVICE};
use crate::flipper::HAROLD;
use crate::protocol::{HEARTBEAT, PACKET_LEN};

pub async fn run(packets: watch::Receiver<[u8; PACKET_LEN]>, usb_busy: watch::Receiver<bool>, verbose: bool) {
    // WinRT calls block, so the link gets a thread of its own.
    let _ = tokio::task::spawn_blocking(move || link(packets, usb_busy, verbose)).await;
}

fn link(mut packets: watch::Receiver<[u8; PACKET_LEN]>, usb_busy: watch::Receiver<bool>, verbose: bool) {
    let mut packet: Option<GattCharacteristic> = None;
    // Duckwalk's notifications, kept alive while the link is.
    let mut _listening: Option<GattCharacteristic> = None;
    // Linked means a write went through, not just that Windows knows him.
    loop {
        if *usb_busy.borrow() {
            packet = None; // USB is doing it
            _listening = None;
            linked(false, true);
            std::thread::sleep(LOOK_AGAIN);
            continue;
        }
        let Some(target) = packet.as_ref() else {
            if let Some((p, n)) = find(verbose).ok().flatten() {
                packet = Some(p);
                _listening = n.filter(|n| listen(n, verbose).unwrap_or(false));
            } else {
                std::thread::sleep(LOOK_AGAIN);
            }
            continue;
        };
        let data = *packets.borrow_and_update();
        if matches!(write(target, &data), Ok(true)) {
            linked(true, false);
            wait_for_news(&mut packets);
        } else {
            linked(false, false);
            packet = None;
            _listening = None;
            std::thread::sleep(LOOK_AGAIN);
        }
    }
}

/// Wait a heartbeat, or less when a new packet comes in (you are active).
fn wait_for_news(packets: &mut watch::Receiver<[u8; PACKET_LEN]>) {
    let until = std::time::Instant::now() + HEARTBEAT;
    while std::time::Instant::now() < until && !packets.has_changed().unwrap_or(false) {
        std::thread::sleep(std::time::Duration::from_millis(20));
    }
}

fn guid(uuid: uuid::Uuid) -> GUID {
    GUID::from_u128(uuid.as_u128())
}

/// Subscribe to Duckwalk's notifications; each one goes to `HAROLD`.
fn listen(notify: &GattCharacteristic, verbose: bool) -> Result<bool> {
    notify.ValueChanged(&TypedEventHandler::new(
        move |_: &Option<GattCharacteristic>, args: &Option<GattValueChangedEventArgs>| {
            if let Some(args) = args {
                let buffer = args.CharacteristicValue()?;
                let mut data = vec![0u8; buffer.Length()? as usize];
                DataReader::FromBuffer(&buffer)?.ReadBytes(&mut data)?;
                HAROLD.receive(&data, verbose);
            }
            Ok(())
        },
    ))?;
    let status = notify
        .WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify,
        )?
        .get()?;
    let ok = status == GattCommunicationStatus::Success;
    if verbose {
        println!(
            "Bluetooth: {}",
            if ok {
                "listening to Duckwalk"
            } else {
                "Duckwalk doesn't talk back"
            }
        );
    }
    Ok(ok)
}

/// Asks the device itself, not Windows' cache: a Duckwalk update can add a
/// characteristic the cache from pairing doesn't have.
fn characteristic(service: &GattDeviceService, uuid: uuid::Uuid) -> Option<GattCharacteristic> {
    let result = service
        .GetCharacteristicsForUuidWithCacheModeAsync(guid(uuid), BluetoothCacheMode::Uncached)
        .and_then(|op| op.get())
        .ok()?;
    if result.Status().ok()? != GattCommunicationStatus::Success {
        return None;
    }
    let list = result.Characteristics().ok()?;
    if list.Size().ok()? == 0 {
        return None;
    }
    list.GetAt(0).ok()
}

/// The packet characteristic of the first paired Harold that answers, and
/// his notify characteristic if his build has one.
fn find(verbose: bool) -> Result<Option<(GattCharacteristic, Option<GattCharacteristic>)>> {
    let selector = GattDeviceService::GetDeviceSelectorFromUuid(guid(SERVICE))?;
    let services = DeviceInformation::FindAllAsyncAqsFilter(&selector)?.get()?;
    for info in services {
        let id = info.Id()?;
        let Ok(service) = GattDeviceService::FromIdAsync(&id).and_then(|op| op.get()) else {
            continue;
        };
        if let Some(packet) = characteristic(&service, PACKET) {
            if verbose {
                println!("Bluetooth: {id}");
            }
            let notify = characteristic(&service, NOTIFY);
            if verbose && notify.is_none() {
                println!("Bluetooth: this Duckwalk build doesn't talk back (no notify)");
            }
            return Ok(Some((packet, notify)));
        }
    }
    Ok(None)
}

fn write(packet: &GattCharacteristic, data: &[u8]) -> Result<bool> {
    let writer = DataWriter::new()?;
    writer.WriteBytes(data)?;
    let buffer = writer.DetachBuffer()?;
    let status = packet
        .WriteValueWithOptionAsync(&buffer, GattWriteOption::WriteWithoutResponse)?
        .get()?;
    Ok(status == GattCommunicationStatus::Success)
}
