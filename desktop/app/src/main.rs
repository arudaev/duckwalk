//! Duckwalk's desktop app: it watches for your own input (not Duckwalk's) and
//! tells Harold over USB or Bluetooth how long you have been idle, so he
//! walks the pointer only while you are away. The packets are the same as the
//! Windows helper's in `desktop/helper`.

mod ble;
mod input;
mod protocol;
mod usb;

use std::time::{Duration, Instant};

use tokio::sync::watch;

use protocol::{idle_packet, usb_report, HEARTBEAT};

const RETRY_OPEN: Duration = Duration::from_secs(2);

const USAGE: &str = "\
Usage: duckwalk [--verbose] [--pretend-working]

Duckwalk's desktop app. Tells Harold over USB or Bluetooth when you use
your own mouse or keyboard, so he walks the pointer only while you are
away. Leave it running.

  --verbose          print what it finds and sends
  --pretend-working  say you are working all the time, to check the link:
                     the Flipper should show \"You're working\"";

/// When you are active, tell Harold straight away, but at most this often.
const SEND_ACTIVE: Duration = Duration::from_millis(250);

fn user_idle(activity: &input::Activity, pretend_working: bool) -> Duration {
    if pretend_working {
        Duration::ZERO
    } else {
        activity.idle()
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

    let activity = input::Activity::new();
    input::start(activity.clone(), verbose);

    let (packet_tx, packet_rx) = watch::channel(idle_packet(user_idle(&activity, pretend_working)));
    let (usb_busy_tx, usb_busy_rx) = watch::channel(false);
    tokio::spawn(ble::run(packet_rx, usb_busy_rx, verbose));

    let mut usb = usb::Usb::new();
    let mut last_try = Instant::now() - RETRY_OPEN;
    let mut ticker = tokio::time::interval(HEARTBEAT);
    let mut sent = Instant::now() - SEND_ACTIVE;
    loop {
        tokio::select! {
            _ = ticker.tick() => {}
            _ = activity.seen() => {
                tokio::time::sleep(SEND_ACTIVE.saturating_sub(sent.elapsed())).await;
            }
        }
        sent = Instant::now();
        let packet = idle_packet(user_idle(&activity, pretend_working));
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
            println!(
                "idle {:.1} s, {} of Duckwalk's own moves ignored",
                ms as f64 / 1000.0,
                input::ignored()
            );
        }
    }
}
