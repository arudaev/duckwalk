//! Bluetooth on macOS and Linux, through btleplug. BlueZ lists paired and
//! connected devices too, so a connected Harold is found without advertising.

use btleplug::api::{Central, CharPropFlags, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::StreamExt;
use tokio::sync::watch;

use super::{linked, LOOK_AGAIN, NAME_PREFIX, NOTIFY, PACKET, SERVICE};
use crate::flipper::HAROLD;
use crate::protocol::{HEARTBEAT, PACKET_LEN};

pub async fn run(mut packets: watch::Receiver<[u8; PACKET_LEN]>, usb_busy: watch::Receiver<bool>, verbose: bool) {
    let adapter = match first_adapter().await {
        Some(a) => a,
        None => {
            if verbose {
                println!("Bluetooth: no adapter, USB only.");
            }
            return;
        }
    };
    let _ = adapter.start_scan(ScanFilter::default()).await;
    loop {
        if *usb_busy.borrow() {
            linked(false, true);
            tokio::time::sleep(LOOK_AGAIN).await;
            continue;
        }
        let Some((harold, packet)) = find(&adapter, verbose).await else {
            tokio::time::sleep(LOOK_AGAIN).await;
            continue;
        };
        listen(&harold, verbose).await;
        // Linked means a write went through, not just that he was found.
        loop {
            if *usb_busy.borrow() {
                break; // USB is doing it
            }
            let data = *packets.borrow();
            if harold.write(&packet, &data, WriteType::WithoutResponse).await.is_err() {
                linked(false, false);
                break;
            }
            linked(true, false);
            // Write at least once a heartbeat, and straight away on news.
            let _ = tokio::time::timeout(HEARTBEAT, packets.changed()).await;
        }
    }
}

/// Subscribe to Duckwalk's notifications, if his build has them; each one
/// goes to `HAROLD` until the link drops.
async fn listen(harold: &Peripheral, verbose: bool) {
    let Some(notify) = harold
        .characteristics()
        .into_iter()
        .find(|c| c.uuid == NOTIFY && c.properties.contains(CharPropFlags::NOTIFY))
    else {
        return;
    };
    let Ok(mut stream) = harold.notifications().await else {
        return;
    };
    if harold.subscribe(&notify).await.is_err() {
        return;
    }
    if verbose {
        println!("Bluetooth: listening to Duckwalk");
    }
    tokio::spawn(async move {
        while let Some(n) = stream.next().await {
            if n.uuid == NOTIFY {
                HAROLD.receive(&n.value, verbose);
            }
        }
    });
}

async fn first_adapter() -> Option<Adapter> {
    let manager = Manager::new().await.ok()?;
    manager.adapters().await.ok()?.into_iter().next()
}

/// Find a Harold that has Duckwalk's service, connect, and return its packet
/// characteristic.
async fn find(adapter: &Adapter, verbose: bool) -> Option<(Peripheral, Characteristic)> {
    for p in adapter.peripherals().await.ok()? {
        let Ok(Some(props)) = p.properties().await else {
            continue;
        };
        let named = props.local_name.as_deref().is_some_and(|n| n.starts_with(NAME_PREFIX));
        if !named && !props.services.contains(&SERVICE) {
            continue;
        }
        if verbose {
            println!(
                "Bluetooth: found {} ({})",
                props.local_name.as_deref().unwrap_or("?"),
                p.address()
            );
        }
        if !p.is_connected().await.unwrap_or(false) && p.connect().await.is_err() {
            continue;
        }
        if p.discover_services().await.is_err() {
            continue;
        }
        let packet = p
            .characteristics()
            .into_iter()
            .find(|c| c.uuid == PACKET && c.properties.contains(CharPropFlags::WRITE_WITHOUT_RESPONSE));
        if let Some(c) = packet {
            return Some((p, c));
        }
    }
    None
}
