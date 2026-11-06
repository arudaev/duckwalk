//! Windows: Raw Input reports every mouse, keyboard and digitizer event with
//! the device it came from (decision record 0001). Events from Duckwalk's own
//! devices are ignored; everything else counts as you, including input made
//! by software, which has no device at all.
//!
//! Duckwalk's devices:
//!   over USB        `HID#VID_1209&PID_0001&Col01#...`
//!   over Bluetooth  `HID#{00001812-...}_80E12780C2E2&Col02#...`, named after
//!                   the address of a paired "Harold <Flipper name>"

use std::cell::RefCell;
use std::collections::HashMap;
use std::sync::atomic::{AtomicIsize, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use windows::core::w;
use windows::Devices::Bluetooth::BluetoothLEDevice;
use windows::Devices::Enumeration::DeviceInformation;
use windows::Win32::Foundation::{HANDLE, HWND, LPARAM, LRESULT, WPARAM};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::UI::Input::{
    GetRawInputData, GetRawInputDeviceInfoW, RegisterRawInputDevices, HRAWINPUT, RAWINPUTDEVICE, RAWINPUTHEADER,
    RIDEV_INPUTSINK, RIDEV_PAGEONLY, RIDI_DEVICENAME, RID_HEADER,
};
use windows::Win32::UI::WindowsAndMessaging::{
    CallNextHookEx, CreateWindowExW, DefWindowProcW, DispatchMessageW, GetMessageW, PostMessageW, RegisterClassW,
    SetWindowsHookExW, TranslateMessage, HC_ACTION, MSG, MSLLHOOKSTRUCT, WH_MOUSE_LL, WINDOW_EX_STYLE, WINDOW_STYLE,
    WM_APP, WM_INPUT, WNDCLASSW,
};

use super::{Activity, HAROLD_NAME, IGNORED};

const DUCKWALK_USB: &str = "vid_1209&pid_0001";
const REFRESH_HAROLD: Duration = Duration::from_secs(30);
const DEVICE_CACHE: usize = 64;

/// Windows marks mouse input it made from touch or pen (MI_WP_SIGNATURE).
const TOUCH_SIGNATURE_MASK: usize = 0xFFFF_FF00;
const TOUCH_SIGNATURE: usize = 0xFF51_5700;
const WM_TOUCH_SEEN: u32 = WM_APP + 1;

/// Is this raw-input device path one of Duckwalk's? `harold` holds the
/// addresses of paired Harolds, as `_80e12780c2e2`.
fn is_duckwalk_path(path: &str, harold: &[String]) -> bool {
    let path = path.to_lowercase();
    path.contains(DUCKWALK_USB) || harold.iter().any(|h| path.contains(h.as_str()))
}

/// `BluetoothLE#BluetoothLE00:11:22:33:44:55-80:e1:27:80:c2:e2` →
/// `_80e12780c2e2`, the form Windows puts in the device's HID paths.
fn address_tag(device_id: &str) -> Option<String> {
    let mac = device_id.rsplit('-').next()?;
    let hex: String = mac.chars().filter(|c| *c != ':').collect::<String>().to_lowercase();
    (hex.len() == 12 && hex.chars().all(|c| c.is_ascii_hexdigit())).then(|| format!("_{hex}"))
}

fn is_touch(extra_info: usize) -> bool {
    extra_info & TOUCH_SIGNATURE_MASK == TOUCH_SIGNATURE
}

/// Every paired "Harold ...", as address tags.
fn paired_harolds(verbose: bool) -> Vec<String> {
    let find = || -> windows::core::Result<Vec<String>> {
        let selector = BluetoothLEDevice::GetDeviceSelectorFromPairingState(true)?;
        let mut found = Vec::new();
        for info in DeviceInformation::FindAllAsyncAqsFilter(&selector)?.get()? {
            let name = info.Name()?.to_string();
            if !name.starts_with(HAROLD_NAME) {
                continue;
            }
            if let Some(tag) = address_tag(&info.Id()?.to_string()) {
                if verbose {
                    println!("paired Duckwalk over Bluetooth: {name} ({})", &tag[1..]);
                }
                found.push(tag);
            }
        }
        Ok(found)
    };
    find().unwrap_or_default()
}

struct State {
    activity: Arc<Activity>,
    verbose: bool,
    harold: Vec<String>,
    harold_version: u64,
    devices: HashMap<isize, bool>,
}

thread_local! {
    static STATE: RefCell<Option<State>> = const { RefCell::new(None) };
}

/// Paired Harolds, kept up to date by a thread of their own: the lookup can
/// block, and the input thread must never wait.
static HAROLDS: Mutex<Vec<String>> = Mutex::new(Vec::new());
static HAROLDS_VERSION: AtomicU64 = AtomicU64::new(0);

fn watch_harolds(verbose: bool) {
    // The first lookup before any input arrives, then every 30 s, quietly.
    *HAROLDS.lock().unwrap() = paired_harolds(verbose);
    HAROLDS_VERSION.fetch_add(1, Ordering::Relaxed);
    std::thread::spawn(move || loop {
        std::thread::sleep(REFRESH_HAROLD);
        let found = paired_harolds(false);
        let mut current = HAROLDS.lock().unwrap();
        if *current != found {
            *current = found;
            HAROLDS_VERSION.fetch_add(1, Ordering::Relaxed);
        }
    });
}

/// The hidden window, for the touch hook to post to.
static WINDOW: AtomicIsize = AtomicIsize::new(0);

impl State {
    fn is_duckwalk(&mut self, device: HANDLE) -> bool {
        self.update_harold();
        if let Some(&duck) = self.devices.get(&(device.0 as isize)) {
            return duck;
        }
        let duck = match device_name(device) {
            Some(name) => {
                let duck = is_duckwalk_path(&name, &self.harold);
                if self.verbose {
                    println!("device {}: {name}", if duck { "Duckwalk (ignored)" } else { "yours" });
                }
                duck
            }
            None => false,
        };
        if self.devices.len() >= DEVICE_CACHE {
            self.devices.clear();
        }
        self.devices.insert(device.0 as isize, duck);
        duck
    }

    /// Pick up a new list of paired Harolds, if there is one.
    fn update_harold(&mut self) {
        let version = HAROLDS_VERSION.load(Ordering::Relaxed);
        if version != self.harold_version {
            self.harold_version = version;
            self.harold = HAROLDS.lock().unwrap().clone();
            self.devices.clear(); // look at every device again
        }
    }
}

fn device_name(device: HANDLE) -> Option<String> {
    let mut buf = [0u16; 512];
    let mut size = buf.len() as u32;
    let n = unsafe { GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, Some(buf.as_mut_ptr().cast()), &mut size) };
    if n == u32::MAX {
        return None;
    }
    let len = buf.iter().position(|&c| c == 0).unwrap_or(buf.len());
    Some(String::from_utf16_lossy(&buf[..len]))
}

fn on_input(input: HRAWINPUT) {
    let mut header = RAWINPUTHEADER::default();
    let mut size = std::mem::size_of::<RAWINPUTHEADER>() as u32;
    let n = unsafe {
        GetRawInputData(
            input,
            RID_HEADER,
            Some((&mut header as *mut RAWINPUTHEADER).cast()),
            &mut size,
            std::mem::size_of::<RAWINPUTHEADER>() as u32,
        )
    };
    if n == u32::MAX {
        return;
    }
    STATE.with_borrow_mut(|state| {
        let Some(state) = state.as_mut() else { return };
        // No device means input made by software: that's you, not Duckwalk.
        if !header.hDevice.is_invalid() && state.is_duckwalk(header.hDevice) {
            IGNORED.fetch_add(1, Ordering::Relaxed);
            return;
        }
        state.activity.user_seen();
    });
}

extern "system" fn window_proc(window: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    match msg {
        WM_INPUT => on_input(HRAWINPUT(lparam.0 as _)),
        WM_TOUCH_SEEN => {
            STATE.with_borrow(|s| s.as_ref().map(|s| s.activity.user_seen()));
            return LRESULT(0);
        }
        _ => {}
    }
    unsafe { DefWindowProcW(window, msg, wparam, lparam) }
}

/// Touch and pen: Windows turns taps and drags on a touch screen into mouse
/// input with a mark of its own, which a low-level hook sees even where the
/// touch screen's reports don't reach Raw Input. The hook only posts a
/// message: it must return quickly.
extern "system" fn touch_hook(code: i32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if code == HC_ACTION as i32 {
        let mouse = unsafe { &*(lparam.0 as *const MSLLHOOKSTRUCT) };
        if is_touch(mouse.dwExtraInfo) {
            let window = HWND(WINDOW.load(Ordering::Relaxed) as _);
            let _ = unsafe { PostMessageW(window, WM_TOUCH_SEEN, WPARAM(0), LPARAM(0)) };
        }
    }
    unsafe { CallNextHookEx(None, code, wparam, lparam) }
}

/// Start watching input on a thread of its own.
pub fn start(activity: Arc<Activity>, verbose: bool) {
    std::thread::spawn(move || {
        if let Err(e) = watch(activity, verbose) {
            println!("Can't watch your input ({e}); Harold walks on his timer.");
        }
    });
}

/// The backstop for touch and pen, on a thread that does nothing else: every
/// mouse move on the computer waits for a low-level hook to return, so its
/// thread must always be free. Raw Input still works without it.
fn touch_backstop(verbose: bool) {
    std::thread::spawn(move || unsafe {
        let Ok(instance) = GetModuleHandleW(None) else { return };
        if SetWindowsHookExW(WH_MOUSE_LL, Some(touch_hook), instance, 0).is_err() {
            if verbose {
                println!("No touch backstop; Raw Input still sees touch.");
            }
            return;
        }
        let mut msg = MSG::default();
        while GetMessageW(&mut msg, None, 0, 0).as_bool() {
            DispatchMessageW(&msg);
        }
    });
}

fn watch(activity: Arc<Activity>, verbose: bool) -> windows::core::Result<()> {
    watch_harolds(verbose);
    unsafe {
        let instance = GetModuleHandleW(None)?;
        let class = WNDCLASSW {
            lpfnWndProc: Some(window_proc),
            hInstance: instance.into(),
            lpszClassName: w!("DuckwalkInput"),
            ..Default::default()
        };
        RegisterClassW(&class);
        // A hidden window: Raw Input with INPUTSINK delivers to a window.
        let window = CreateWindowExW(
            WINDOW_EX_STYLE(0),
            class.lpszClassName,
            w!("Duckwalk input"),
            WINDOW_STYLE(0),
            0,
            0,
            0,
            0,
            None,
            None,
            instance,
            None,
        )?;
        WINDOW.store(window.0 as isize, Ordering::Relaxed);

        // Gestures on a precision touchpad, touch screens and pens never show
        // up as mouse input, so listen to every digitizer too.
        let wanted = [
            RAWINPUTDEVICE {
                usUsagePage: 0x01,
                usUsage: 0x02,
                dwFlags: RIDEV_INPUTSINK,
                hwndTarget: window,
            },
            RAWINPUTDEVICE {
                usUsagePage: 0x01,
                usUsage: 0x06,
                dwFlags: RIDEV_INPUTSINK,
                hwndTarget: window,
            },
            RAWINPUTDEVICE {
                usUsagePage: 0x0D,
                usUsage: 0,
                dwFlags: RIDEV_PAGEONLY | RIDEV_INPUTSINK,
                hwndTarget: window,
            },
        ];
        RegisterRawInputDevices(&wanted, std::mem::size_of::<RAWINPUTDEVICE>() as u32)?;

        let state = State {
            activity: activity.clone(),
            verbose,
            harold: Vec::new(),
            harold_version: 0,
            devices: HashMap::new(),
        };
        STATE.set(Some(state));
        activity.user_seen();
        activity.set_watching();

        touch_backstop(verbose);

        let mut msg = MSG::default();
        while GetMessageW(&mut msg, None, 0, 0).as_bool() {
            let _ = TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    Ok(())
}

/// How many of Duckwalk's own moves were ignored, for `--verbose`.
pub fn ignored() -> u64 {
    IGNORED.load(Ordering::Relaxed)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn usb_paths_are_duckwalk() {
        assert!(is_duckwalk_path(
            r"\\?\HID#VID_1209&PID_0001&Col01#7&1&0&0000#{378de44c}",
            &[]
        ));
        assert!(!is_duckwalk_path(
            r"\\?\HID#VID_045E&PID_0932&Col01#7&1&0&0000#{378de44c}",
            &[]
        ));
    }

    #[test]
    fn bluetooth_paths_match_the_address() {
        let harold = vec!["_80e12780c2e2".to_string()];
        let path = r"\\?\HID#{00001812-0000-1000-8000-00805f9b34fb}_80E12780C2E2&Col02#8&2&0&0001#{378de44c}";
        assert!(is_duckwalk_path(path, &harold));
        let other = r"\\?\HID#{00001812-0000-1000-8000-00805f9b34fb}_DFCF0DCD424F&Col02#8&2&0&0001#{378de44c}";
        assert!(!is_duckwalk_path(other, &harold));
    }

    #[test]
    fn address_tag_from_device_id() {
        assert_eq!(
            address_tag("BluetoothLE#BluetoothLE00:11:22:33:44:55-80:e1:27:80:c2:e2").as_deref(),
            Some("_80e12780c2e2")
        );
        assert_eq!(address_tag("nonsense"), None);
    }

    #[test]
    fn touch_signature() {
        assert!(is_touch(0xFF51_5780));
        assert!(!is_touch(0));
    }
}
