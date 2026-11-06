//! Duckwalk's desktop app. For now (issue #119) the skeleton: it finds Harold
//! over USB or Bluetooth and sends the same idle packets as the Windows
//! helper in `desktop/helper`. Seeing your own input comes next (#120); until
//! then the app reports "away", so Harold walks as he would without it.

mod ble;
mod protocol;
mod usb;

use std::time::{Duration, Instant};

use tokio::sync::watch;

use protocol::{idle_packet, usb_report, HEARTBEAT};

const RETRY_OPEN: Duration = Duration::from_secs(2);

const USAGE: &str = "\
Usage: duckwalk [--verbose] [--pretend-working]

Duckwalk's desktop app. Finds Harold over USB or Bluetooth and tells him
how long you have been away. Leave it running.

  --verbose          print what it finds and sends
  --pretend-working  say you are working all the time, to check the link:
                     the Flipper should show \"You're working\"";

/// Where the user's idle time comes from. Until #120 brings input on every
/// system, the skeleton knows of no input and reports "away".
fn user_idle(pretend_working: bool) -> Duration {
    if pretend_working {
        Duration::ZERO
    } else {
        Duration::from_millis(u32::MAX as u64)
    }
}

#[tokio::main]
async fn main() {
    let mut verbose = false;
    let mut pretend_working = false;
    for arg in std::env::args().skip(1) {
        match arg.as_str() {
            "--verbose" | "-v" => verbose = true,
            "--pretend-working" => pretend_working = true,
            _ => {
                println!("{USAGE}");
                return;
            }
        }
    }

    println!(
        "Duckwalk desktop app {}: looking for Harold over USB and Bluetooth.",
        env!("CARGO_PKG_VERSION")
    );

    let (packet_tx, packet_rx) = watch::channel(idle_packet(user_idle(pretend_working)));
    let (usb_busy_tx, usb_busy_rx) = watch::channel(false);
    tokio::spawn(ble::run(packet_rx, usb_busy_rx, verbose));

    let mut usb = usb::Usb::new();
    let mut last_try = Instant::now() - RETRY_OPEN;
    let mut ticker = tokio::time::interval(HEARTBEAT);
    loop {
        ticker.tick().await;
        let packet = idle_packet(user_idle(pretend_working));
        packet_tx.send_replace(packet);

        if !usb.is_open() && last_try.elapsed() >= RETRY_OPEN {
            last_try = Instant::now();
            if usb.open(verbose) {
                println!("Connected to Duckwalk over USB.");
            }
        }
        if usb.is_open() && !usb.send(&usb_report(&packet)) {
            println!("Lost Duckwalk over USB (unplugged, or closed). Waiting for it...");
        }
        usb_busy_tx.send_if_modified(|busy| {
            let changed = *busy != usb.is_open();
            *busy = usb.is_open();
            changed
        });
        if verbose {
            let ms = u32::from_le_bytes([packet[3], packet[4], packet[5], packet[6]]);
            println!("idle {ms} ms");
        }
    }
}
