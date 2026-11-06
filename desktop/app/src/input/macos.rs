//! macOS: IOHIDManager reports every value from every keyboard, mouse,
//! trackpad and digitizer, with the device it came from. Duckwalk's own
//! devices are skipped: over USB by vendor and product ID, over Bluetooth by
//! the name it pairs under, "Harold <Flipper name>".
//!
//! Reading input needs the Input Monitoring permission (System Settings >
//! Privacy & Security > Input Monitoring). The app asks for it on first start;
//! without it the app says so and reports "away".

use std::collections::HashMap;
use std::ffi::{c_char, c_void, CStr, CString};
use std::sync::atomic::Ordering;
use std::sync::Arc;

use super::{is_duckwalk_device, Activity, IGNORED};

type CFTypeRef = *const c_void;
type CFStringRef = *const c_void;
type CFNumberRef = *const c_void;
type CFDictionaryRef = *const c_void;
type CFArrayRef = *const c_void;
type CFRunLoopRef = *const c_void;
type IOHIDManagerRef = *const c_void;
type IOHIDValueRef = *const c_void;
type IOHIDElementRef = *const c_void;
type IOHIDDeviceRef = *const c_void;
type IOReturn = i32;

const K_CF_STRING_ENCODING_UTF8: u32 = 0x0800_0100;
const K_CF_NUMBER_SINT32_TYPE: i32 = 3;
const K_IOHID_REQUEST_TYPE_LISTEN_EVENT: u32 = 1;
const K_IOHID_ACCESS_TYPE_GRANTED: u32 = 0;
const K_IO_RETURN_SUCCESS: IOReturn = 0;

#[repr(C)]
struct CFDictionaryCallBacks([usize; 6]);
#[repr(C)]
struct CFArrayCallBacks([usize; 5]);

#[link(name = "CoreFoundation", kind = "framework")]
extern "C" {
    static kCFRunLoopDefaultMode: CFStringRef;
    static kCFTypeDictionaryKeyCallBacks: CFDictionaryCallBacks;
    static kCFTypeDictionaryValueCallBacks: CFDictionaryCallBacks;
    static kCFTypeArrayCallBacks: CFArrayCallBacks;
    fn CFRunLoopGetCurrent() -> CFRunLoopRef;
    fn CFRunLoopRun();
    fn CFStringCreateWithCString(alloc: CFTypeRef, s: *const c_char, encoding: u32) -> CFStringRef;
    fn CFStringGetCString(s: CFStringRef, buf: *mut c_char, size: isize, encoding: u32) -> bool;
    fn CFNumberCreate(alloc: CFTypeRef, kind: i32, value: *const c_void) -> CFNumberRef;
    fn CFNumberGetValue(n: CFNumberRef, kind: i32, value: *mut c_void) -> bool;
    fn CFDictionaryCreate(
        alloc: CFTypeRef,
        keys: *const CFTypeRef,
        values: *const CFTypeRef,
        count: isize,
        key_callbacks: *const CFDictionaryCallBacks,
        value_callbacks: *const CFDictionaryCallBacks,
    ) -> CFDictionaryRef;
    fn CFArrayCreate(
        alloc: CFTypeRef,
        values: *const CFTypeRef,
        count: isize,
        callbacks: *const CFArrayCallBacks,
    ) -> CFArrayRef;
    fn CFGetTypeID(t: CFTypeRef) -> usize;
    fn CFNumberGetTypeID() -> usize;
    fn CFStringGetTypeID() -> usize;
}

type InputValueCallback =
    extern "C" fn(context: *mut c_void, result: IOReturn, sender: *mut c_void, value: IOHIDValueRef);

#[link(name = "IOKit", kind = "framework")]
extern "C" {
    fn IOHIDManagerCreate(alloc: CFTypeRef, options: u32) -> IOHIDManagerRef;
    fn IOHIDManagerSetDeviceMatchingMultiple(manager: IOHIDManagerRef, multiple: CFArrayRef);
    fn IOHIDManagerRegisterInputValueCallback(
        manager: IOHIDManagerRef,
        callback: InputValueCallback,
        context: *mut c_void,
    );
    fn IOHIDManagerScheduleWithRunLoop(manager: IOHIDManagerRef, run_loop: CFRunLoopRef, mode: CFStringRef);
    fn IOHIDManagerOpen(manager: IOHIDManagerRef, options: u32) -> IOReturn;
    fn IOHIDValueGetElement(value: IOHIDValueRef) -> IOHIDElementRef;
    fn IOHIDElementGetDevice(element: IOHIDElementRef) -> IOHIDDeviceRef;
    fn IOHIDDeviceGetProperty(device: IOHIDDeviceRef, key: CFStringRef) -> CFTypeRef;
    fn IOHIDCheckAccess(request: u32) -> u32;
    fn IOHIDRequestAccess(request: u32) -> bool;
}

fn cf_string(s: &str) -> CFStringRef {
    let c = CString::new(s).unwrap();
    unsafe { CFStringCreateWithCString(std::ptr::null(), c.as_ptr(), K_CF_STRING_ENCODING_UTF8) }
}

fn cf_number(n: i32) -> CFNumberRef {
    unsafe { CFNumberCreate(std::ptr::null(), K_CF_NUMBER_SINT32_TYPE, (&n as *const i32).cast()) }
}

/// Match devices by usage page and usage; usage `None` takes the whole page.
fn matching(page: i32, usage: Option<i32>) -> CFDictionaryRef {
    let mut keys = vec![cf_string("DeviceUsagePage")];
    let mut values = vec![cf_number(page)];
    if let Some(u) = usage {
        keys.push(cf_string("DeviceUsage"));
        values.push(cf_number(u));
    }
    unsafe {
        CFDictionaryCreate(
            std::ptr::null(),
            keys.as_ptr(),
            values.as_ptr(),
            keys.len() as isize,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks,
        )
    }
}

fn device_number(device: IOHIDDeviceRef, key: &str) -> u16 {
    unsafe {
        let n = IOHIDDeviceGetProperty(device, cf_string(key));
        let mut v: i32 = 0;
        if !n.is_null() && CFGetTypeID(n) == CFNumberGetTypeID() {
            CFNumberGetValue(n, K_CF_NUMBER_SINT32_TYPE, (&mut v as *mut i32).cast());
        }
        v as u16
    }
}

fn device_string(device: IOHIDDeviceRef, key: &str) -> String {
    unsafe {
        let s = IOHIDDeviceGetProperty(device, cf_string(key));
        if s.is_null() || CFGetTypeID(s) != CFStringGetTypeID() {
            return String::new();
        }
        let mut buf = [0 as c_char; 256];
        if !CFStringGetCString(s, buf.as_mut_ptr(), buf.len() as isize, K_CF_STRING_ENCODING_UTF8) {
            return String::new();
        }
        CStr::from_ptr(buf.as_ptr()).to_string_lossy().into_owned()
    }
}

struct Context {
    activity: Arc<Activity>,
    verbose: bool,
    devices: HashMap<usize, bool>,
}

extern "C" fn on_value(context: *mut c_void, _result: IOReturn, _sender: *mut c_void, value: IOHIDValueRef) {
    let ctx = unsafe { &mut *(context as *mut Context) };
    let device = unsafe { IOHIDElementGetDevice(IOHIDValueGetElement(value)) };
    let verbose = ctx.verbose;
    let duck = *ctx.devices.entry(device as usize).or_insert_with(|| {
        let name = device_string(device, "Product");
        let duck = is_duckwalk_device(
            device_number(device, "VendorID"),
            device_number(device, "ProductID"),
            &name,
        );
        if verbose {
            println!("device {}: {name}", if duck { "Duckwalk (ignored)" } else { "yours" });
        }
        duck
    });
    if duck {
        IGNORED.fetch_add(1, Ordering::Relaxed);
    } else {
        ctx.activity.user_seen();
    }
}

pub fn start(activity: Arc<Activity>, verbose: bool) {
    std::thread::spawn(move || unsafe {
        if IOHIDCheckAccess(K_IOHID_REQUEST_TYPE_LISTEN_EVENT) != K_IOHID_ACCESS_TYPE_GRANTED {
            println!(
                "Duckwalk needs Input Monitoring to tell your own mouse and keyboard from Harold's. \
                 It reads only that an input happened and from which device, never which keys. \
                 Allow it in System Settings > Privacy & Security > Input Monitoring, then start Duckwalk again."
            );
            if !IOHIDRequestAccess(K_IOHID_REQUEST_TYPE_LISTEN_EVENT) {
                println!("Until then Harold walks on his timer.");
                return;
            }
        }

        let manager = IOHIDManagerCreate(std::ptr::null(), 0);
        // Mice, pointers and keyboards, and every digitizer (trackpad, touch
        // screen, pen). Not the Mac's other sensors, which report all the time.
        let wanted = [
            matching(0x01, Some(0x02)),
            matching(0x01, Some(0x01)),
            matching(0x01, Some(0x06)),
            matching(0x0D, None),
        ];
        let array = CFArrayCreate(
            std::ptr::null(),
            wanted.as_ptr(),
            wanted.len() as isize,
            &kCFTypeArrayCallBacks,
        );
        IOHIDManagerSetDeviceMatchingMultiple(manager, array);

        let context = Box::into_raw(Box::new(Context {
            activity: activity.clone(),
            verbose,
            devices: HashMap::new(),
        }));
        IOHIDManagerRegisterInputValueCallback(manager, on_value, context.cast());
        IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        if IOHIDManagerOpen(manager, 0) != K_IO_RETURN_SUCCESS {
            println!("Can't watch your input (Input Monitoring not allowed?); Harold walks on his timer.");
            return;
        }
        activity.user_seen();
        activity.set_watching();
        CFRunLoopRun();
    });
}

pub fn ignored() -> u64 {
    IGNORED.load(Ordering::Relaxed)
}
