//! Bluetooth on Windows, through WinRT. btleplug finds only devices that
//! advertise, and a connected Harold doesn't; Windows lists the GATT service
//! of every paired device instead, which is how the C helper finds him too.

use tokio::sync::watch;
use windows::core::{Result, GUID};
use windows::Devices::Bluetooth::GenericAttributeProfile::{
    GattCharacteristic, GattCommunicationStatus, GattDeviceService, GattWriteOption,
};
use windows::Devices::Enumeration::DeviceInformation;
use windows::Storage::Streams::DataWriter;

use super::{CONNECTED, LOOK_AGAIN, LOST, PACKET, SERVICE};
use crate::protocol::{HEARTBEAT, PACKET_LEN};

pub async fn run(packets: watch::Receiver<[u8; PACKET_LEN]>, usb_busy: watch::Receiver<bool>, verbose: bool) {
    // WinRT calls block, so the link gets a thread of its own.
    let _ = tokio::task::spawn_blocking(move || link(packets, usb_busy, verbose)).await;
}

fn link(packets: watch::Receiver<[u8; PACKET_LEN]>, usb_busy: watch::Receiver<bool>, verbose: bool) {
    let mut packet: Option<GattCharacteristic> = None;
    // Connected means a write went through, not just that Windows knows him.
    let mut connected = false;
    loop {
        if *usb_busy.borrow() {
            packet = None; // USB is doing it
            connected = false;
            std::thread::sleep(LOOK_AGAIN);
            continue;
        }
        let Some(target) = packet.as_ref() else {
            packet = find(verbose).ok().flatten();
            if packet.is_none() {
                std::thread::sleep(LOOK_AGAIN);
            }
            continue;
        };
        let data = *packets.borrow();
        if matches!(write(target, &data), Ok(true)) {
            if !connected {
                println!("{CONNECTED}");
                connected = true;
            }
            std::thread::sleep(HEARTBEAT);
        } else {
            if connected {
                println!("{LOST}");
                connected = false;
            }
            packet = None;
            std::thread::sleep(LOOK_AGAIN);
        }
    }
}

fn guid(uuid: uuid::Uuid) -> GUID {
    GUID::from_u128(uuid.as_u128())
}

/// The packet characteristic of the first paired Harold that answers.
fn find(verbose: bool) -> Result<Option<GattCharacteristic>> {
    let selector = GattDeviceService::GetDeviceSelectorFromUuid(guid(SERVICE))?;
    let services = DeviceInformation::FindAllAsyncAqsFilter(&selector)?.get()?;
    for info in services {
        let id = info.Id()?;
        let Ok(service) = GattDeviceService::FromIdAsync(&id).and_then(|op| op.get()) else {
            continue;
        };
        let Ok(result) = service
            .GetCharacteristicsForUuidAsync(guid(PACKET))
            .and_then(|op| op.get())
        else {
            continue;
        };
        if result.Status()? != GattCommunicationStatus::Success {
            continue;
        }
        let list = result.Characteristics()?;
        if list.Size()? > 0 {
            if verbose {
                println!("Bluetooth: {id}");
            }
            return Ok(Some(list.GetAt(0)?));
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
