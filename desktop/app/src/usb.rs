//! Duckwalk over USB: the data channel of the composite HID device.

use hidapi::{HidApi, HidDevice};

/// pid.codes test IDs for now (see `app/usb/usb_duck.h`).
pub const VID: u16 = 0x1209;
pub const PID: u16 = 0x0001;

/// The data channel is a vendor-defined collection (usage page 0xFF00 and
/// up). Windows lists every collection on its own; macOS and Linux list the
/// interface once, and any of its entries takes report 4.
fn is_data_channel(usage_page: u16) -> bool {
    usage_page >= 0xFF00
}

pub struct Usb {
    api: Option<HidApi>,
    device: Option<HidDevice>,
}

impl Usb {
    pub fn new() -> Self {
        Usb {
            api: HidApi::new().ok(),
            device: None,
        }
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
        if let Some(info) = pick {
            if let Ok(device) = info.open_device(api) {
                if verbose {
                    println!("USB: {}", info.path().to_string_lossy());
                }
                self.device = Some(device);
            }
        }
        self.device.is_some()
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

#[cfg(test)]
mod tests {
    #[test]
    fn data_channel_is_the_vendor_page() {
        assert!(super::is_data_channel(0xFF00));
        assert!(!super::is_data_channel(0x0001));
        assert!(!super::is_data_channel(0x000C));
    }
}
