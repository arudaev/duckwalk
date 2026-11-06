//! Duckwalk over Bluetooth: Duckwalk's own GATT service next to the mouse
//! (see `app/ble_duck.c`). The app writes each packet to one characteristic,
//! and Duckwalk notifies it on the same one (flipper.rs reads those).
//!
//! `run` keeps a link to Duckwalk while USB is not in use and writes the
//! newest packet at least once a heartbeat. It runs until the program ends
//! and prints when the link comes and goes.

use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use uuid::{uuid, Uuid};

pub const SERVICE: Uuid = uuid!("15f18352-9e6d-4ff4-9f45-cd158a70941c");
pub const PACKET: Uuid = uuid!("15f18352-9e6d-4ff4-9f45-cd158a70941d");
/// Duckwalk's own packets arrive as notifications on the packet
/// characteristic; older Flipper builds don't offer notify on it.
pub const NOTIFY: Uuid = PACKET;

/// Duckwalk pairs as "Harold <Flipper name>".
#[cfg_attr(windows, allow(dead_code))]
pub const NAME_PREFIX: &str = "Harold ";

const LOOK_AGAIN: Duration = Duration::from_secs(5);

/// Is a Bluetooth link up (a write went through)? Read for the tray.
pub static LINKED: AtomicBool = AtomicBool::new(false);

/// The link came up (`true`) or went (`false`); say so once.
fn linked(up: bool, quiet: bool) {
    if LINKED.swap(up, Ordering::Relaxed) == up || quiet {
        return;
    }
    if up {
        println!("Connected to Duckwalk over Bluetooth.");
    } else {
        println!("Lost Duckwalk over Bluetooth (out of range, or closed). Waiting for it...");
    }
}

#[cfg(not(windows))]
mod btleplug;
#[cfg(not(windows))]
pub use self::btleplug::run;

#[cfg(windows)]
mod winrt;
#[cfg(windows)]
pub use self::winrt::run;
