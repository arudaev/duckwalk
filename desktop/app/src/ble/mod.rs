//! Duckwalk over Bluetooth: Duckwalk's own GATT service next to the mouse
//! (see `app/ble_duck.c`). The app writes each packet to one characteristic.
//!
//! `run` keeps a link to Duckwalk while USB is not in use and writes the
//! newest packet at least once a heartbeat. It runs until the program ends
//! and prints when the link comes and goes.

use std::time::Duration;

use uuid::{uuid, Uuid};

pub const SERVICE: Uuid = uuid!("15f18352-9e6d-4ff4-9f45-cd158a70941c");
pub const PACKET: Uuid = uuid!("15f18352-9e6d-4ff4-9f45-cd158a70941d");

/// Duckwalk pairs as "Harold <Flipper name>".
#[cfg_attr(windows, allow(dead_code))]
pub const NAME_PREFIX: &str = "Harold ";

const LOOK_AGAIN: Duration = Duration::from_secs(5);

const CONNECTED: &str = "Connected to Duckwalk over Bluetooth.";
const LOST: &str = "Lost Duckwalk over Bluetooth (out of range, or closed). Waiting for it...";

#[cfg(not(windows))]
mod btleplug;
#[cfg(not(windows))]
pub use self::btleplug::run;

#[cfg(windows)]
mod winrt;
#[cfg(windows)]
pub use self::winrt::run;
