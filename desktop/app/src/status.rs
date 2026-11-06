//! What the tray shows: how Harold is connected and whether you're working.
//! Written by the link threads, read by the tray.

use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU8, Ordering};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Connection {
    None,
    Usb,
    Bluetooth,
}

/// Input this recent counts as "you're working" in the tray. Harold himself
/// waits as long as Resume after says, which the app doesn't know.
const WORKING_S: u32 = 5;
/// Idle time when input can't be watched at all.
pub const UNWATCHED: u32 = u32::MAX;

pub struct Status {
    connection: AtomicU8,
    idle_s: AtomicU32,
    asleep: AtomicBool,
}

impl Status {
    pub const fn new() -> Self {
        Status {
            connection: AtomicU8::new(0),
            idle_s: AtomicU32::new(UNWATCHED),
            asleep: AtomicBool::new(false),
        }
    }

    pub fn set_connection(&self, c: Connection) {
        self.connection.store(c as u8, Ordering::Relaxed);
    }

    pub fn connection(&self) -> Connection {
        match self.connection.load(Ordering::Relaxed) {
            1 => Connection::Usb,
            2 => Connection::Bluetooth,
            _ => Connection::None,
        }
    }

    /// Duckwalk said Harold is asleep (keep-awake off).
    pub fn set_asleep(&self, asleep: bool) {
        self.asleep.store(asleep, Ordering::Relaxed);
    }

    pub fn set_idle_s(&self, s: u32) {
        self.idle_s.store(s, Ordering::Relaxed);
    }

    /// One line for the tray's tooltip and menu.
    pub fn line(&self) -> String {
        let idle = self.idle_s.load(Ordering::Relaxed);
        let you = match idle {
            UNWATCHED => "can't see your input".to_string(),
            s if s < WORKING_S => "you're working".to_string(),
            s if s < 60 => format!("idle {s} s"),
            s if s < 3600 => format!("idle {} min", s / 60),
            s => format!("idle {} h", s / 3600),
        };
        let asleep = if self.asleep.load(Ordering::Relaxed) {
            ", asleep"
        } else {
            ""
        };
        match self.connection() {
            Connection::None => format!("Looking for Harold; {you}"),
            Connection::Usb => format!("Harold on USB{asleep}; {you}"),
            Connection::Bluetooth => format!("Harold on Bluetooth{asleep}; {you}"),
        }
    }
}

/// The one status of this app.
pub static STATUS: Status = Status::new();

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lines() {
        let s = Status::new();
        assert_eq!(s.line(), "Looking for Harold; can't see your input");
        s.set_connection(Connection::Usb);
        s.set_idle_s(1);
        assert_eq!(s.line(), "Harold on USB; you're working");
        s.set_connection(Connection::Bluetooth);
        s.set_idle_s(42);
        assert_eq!(s.line(), "Harold on Bluetooth; idle 42 s");
        s.set_idle_s(600);
        assert_eq!(s.line(), "Harold on Bluetooth; idle 10 min");
        s.set_asleep(true);
        assert_eq!(s.line(), "Harold on Bluetooth, asleep; idle 10 min");
    }
}
