//! What the desktop app and the Flipper say to each other. The same format as
//! `app/helper_protocol.h`; keep the two in step.
//!
//! Every packet is 63 bytes: `[0] 'D'  [1] version  [2] message type  [3..] data`.

use std::time::Duration;

pub const MAGIC: u8 = b'D';
pub const VERSION: u8 = 1;
pub const MSG_IDLE: u8 = 0x01;
/// The app is quitting: the Flipper drops "+PC" at once.
pub const MSG_BYE: u8 = 0x02;
/// Wake, sleep or a mode, from an agent through `duckwalk mcp`.
pub const MSG_COMMAND: u8 = 0x03;
pub const PACKET_LEN: usize = 63;

/// Over USB the packet travels as vendor report 4.
pub const USB_REPORT_ID: u8 = 4;

/// How often the app reports at least; the Flipper drops the "+PC" after 5 s.
pub const HEARTBEAT: Duration = Duration::from_millis(1000);

/// An idle packet: milliseconds since the user last touched their own mouse
/// or keyboard, little-endian, capped at `u32::MAX`.
pub fn idle_packet(idle: Duration) -> [u8; PACKET_LEN] {
    let ms = u32::try_from(idle.as_millis()).unwrap_or(u32::MAX);
    let mut p = [0u8; PACKET_LEN];
    p[0] = MAGIC;
    p[1] = VERSION;
    p[2] = MSG_IDLE;
    p[3..7].copy_from_slice(&ms.to_le_bytes());
    p
}

/// Goodbye: the app is quitting.
pub fn bye_packet() -> [u8; PACKET_LEN] {
    let mut p = [0u8; PACKET_LEN];
    p[0] = MAGIC;
    p[1] = VERSION;
    p[2] = MSG_BYE;
    p
}

/// A command for Harold, as if pressed on the Flipper (`HELPER_MSG_COMMAND`).
pub fn command_packet(cmd: &crate::control::Command) -> [u8; PACKET_LEN] {
    use crate::control::Command;
    let mut p = [0u8; PACKET_LEN];
    p[0] = MAGIC;
    p[1] = VERSION;
    p[2] = MSG_COMMAND;
    (p[3], p[4]) = match *cmd {
        Command::Wake => (1, 0),
        Command::Sleep => (2, 0),
        Command::Mode(m) => (3, m),
    };
    p
}

/// The same packet with the USB report ID in front, ready for `hid_write`.
pub fn usb_report(packet: &[u8; PACKET_LEN]) -> [u8; PACKET_LEN + 1] {
    let mut r = [0u8; PACKET_LEN + 1];
    r[0] = USB_REPORT_ID;
    r[1..].copy_from_slice(packet);
    r
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn idle_packet_matches_the_c_helper() {
        let p = idle_packet(Duration::from_millis(0x0102_0304));
        assert_eq!(&p[..7], &[b'D', 1, 0x01, 0x04, 0x03, 0x02, 0x01]);
        assert!(p[7..].iter().all(|&b| b == 0));
        assert_eq!(p.len(), 63);
    }

    #[test]
    fn bye_packet_has_no_data() {
        let p = bye_packet();
        assert_eq!(&p[..3], &[b'D', 1, 0x02]);
        assert!(p[3..].iter().all(|&b| b == 0));
    }

    #[test]
    fn command_packets() {
        use crate::control::Command;
        assert_eq!(&command_packet(&Command::Sleep)[..5], &[b'D', 1, 0x03, 2, 0]);
        assert_eq!(&command_packet(&Command::Mode(3))[..5], &[b'D', 1, 0x03, 3, 3]);
    }

    #[test]
    fn idle_is_capped() {
        let p = idle_packet(Duration::from_secs(60 * 60 * 24 * 365));
        assert_eq!(&p[3..7], &[0xFF; 4]);
    }

    #[test]
    fn usb_report_puts_the_id_first() {
        let r = usb_report(&idle_packet(Duration::ZERO));
        assert_eq!(r.len(), 64);
        assert_eq!(&r[..4], &[4, b'D', 1, 0x01]);
    }
}
