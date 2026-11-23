//! Duckwalk over USB: the data channel of the composite HID device.

use hidapi::{HidApi, HidDevice};

/// Duckwalk's pid.codes ID (see `app/usb/usb_duck.h`).
pub const VID: u16 = 0x1209;
pub const PID: u16 = 0xD0C4;

/// Where Linux users find the udev rule that lets them open Duckwalk.
pub const LINUX_RULE_HELP: &str = "install the udev rule desktop/app/linux/70-duckwalk.rules, then unplug the Flipper and plug it back in: https://github.com/arudaev/duckwalk/blob/main/docs/install.md#over-usb-let-the-app-reach-duckwalk";

/// The data channel is a vendor-defined collection (usage page 0xFF00 and
/// up). Windows lists every collection on its own; macOS and Linux list the
/// interface once, and any of its entries takes report 4.
fn is_data_channel(usage_page: u16) -> bool {
    usage_page >= 0xFF00
}

pub struct Usb {
    api: Option<HidApi>,
    device: Option<HidDevice>,
    /// Duckwalk is plugged in, but this user may not open it (Linux hidraw).
    denied: bool,
}

impl Usb {
    pub fn new() -> Self {
        Usb {
            api: HidApi::new().ok(),
            device: None,
            denied: false,
        }
    }

    /// Duckwalk is plugged in, but the system won't let this user open it:
    /// on Linux, its `/dev/hidraw*` node without the udev rule.
    pub fn denied(&self) -> bool {
        self.denied
    }

    pub fn is_open(&self) -> bool {
        self.device.is_some()
    }

    /// Look for Duckwalk and open its data channel.
    pub fn open(&mut self, verbose: bool) -> bool {
        let Some(api) = self.api.as_mut() else { return false };
        if api.refresh_devices().is_err() {
            return false;
        }
        let ours: Vec<_> = api
            .device_list()
            .filter(|d| d.vendor_id() == VID && d.product_id() == PID)
            .collect();
        let pick = ours
            .iter()
            .find(|d| is_data_channel(d.usage_page()))
            .or(if cfg!(windows) { None } else { ours.first() });
        let mut denied = false;
        if let Some(info) = pick {
            match info.open_device(api) {
                Ok(device) => {
                    if verbose {
                        println!("USB: {}", info.path().to_string_lossy());
                    }
                    self.device = Some(device);
                }
                Err(_) => denied = permission_denied(info.path()),
            }
        }
        if denied && !self.denied {
            println!(
                "Found Duckwalk over USB, but this user may not open {}: {LINUX_RULE_HELP}",
                pick.map(|i| i.path().to_string_lossy()).unwrap_or_default()
            );
        }
        self.denied = denied;
        self.device.is_some()
    }

    /// Hand every packet Duckwalk sent since the last call to `receive`.
    /// hidapi gives the report ID first, as it was on the wire.
    pub fn poll(&mut self, mut receive: impl FnMut(&[u8])) {
        let Some(device) = self.device.as_ref() else { return };
        let mut buf = [0u8; 65];
        while let Ok(n) = device.read_timeout(&mut buf, 0) {
            if n == 0 {
                break;
            }
            match buf[..n] {
                [crate::protocol::USB_REPORT_ID, ref packet @ ..] => receive(packet),
                ref packet => receive(packet),
            }
        }
    }

    /// Send one report; on failure close, so the next tick looks again.
    pub fn send(&mut self, report: &[u8]) -> bool {
        let ok = self.device.as_ref().is_some_and(|d| d.write(report).is_ok());
        if !ok {
            self.device = None;
        }
        ok
    }
}

/// Whether the device node at `path` refuses this user. hidapi's own error
/// doesn't say why an open failed, so ask the file system. Only Linux opens
/// a plain device file; elsewhere access isn't the usual reason.
#[cfg(target_os = "linux")]
fn permission_denied(path: &std::ffi::CStr) -> bool {
    use std::os::unix::ffi::OsStrExt;
    let path = std::path::Path::new(std::ffi::OsStr::from_bytes(path.to_bytes()));
    std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .open(path)
        .is_err_and(|e| e.kind() == std::io::ErrorKind::PermissionDenied)
}

#[cfg(not(target_os = "linux"))]
fn permission_denied(_path: &std::ffi::CStr) -> bool {
    false
}

#[cfg(test)]
mod tests {
    #[test]
    fn data_channel_is_the_vendor_page() {
        assert!(super::is_data_channel(0xFF00));
        assert!(!super::is_data_channel(0x0001));
        assert!(!super::is_data_channel(0x000C));
    }

    /// Without the udev rule, Duckwalk's hidraw node is like this file: there,
    /// but not for this user. Root may open anything, so it isn't denied.
    #[cfg(target_os = "linux")]
    #[test]
    fn a_node_this_user_may_not_open_is_denied() {
        use std::os::unix::fs::{MetadataExt, PermissionsExt};
        let path = std::env::temp_dir().join(format!("duckwalk-hidraw-{}", std::process::id()));
        std::fs::write(&path, b"").unwrap();
        std::fs::set_permissions(&path, std::fs::Permissions::from_mode(0o000)).unwrap();
        let root = std::fs::metadata("/proc/self").unwrap().uid() == 0;
        let c_path = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
        assert_eq!(super::permission_denied(&c_path), !root);
        assert!(!super::permission_denied(c"/dev/duckwalk-missing"));
        std::fs::remove_file(&path).unwrap();
    }
}
