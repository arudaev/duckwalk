//! Linux: every input device is a file under `/dev/input/` (evdev), and the
//! app reads the ones that are yours. Duckwalk's own devices are skipped:
//! over USB by vendor and product ID, over Bluetooth by the name it pairs
//! under, "Harold <Flipper name>".
//!
//! Reading `/dev/input/event*` needs the `input` group: `sudo usermod -aG
//! input $USER`, then log out and in again. Without it nothing can be read,
//! and the app says so and reports "away".

use std::collections::HashSet;
use std::path::PathBuf;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use evdev::{Device, EventType};

use super::{is_duckwalk_device, Activity, IGNORED};

/// New devices (a mouse plugged in, Harold paired) are picked up this often.
const RESCAN: Duration = Duration::from_secs(5);

pub fn start(activity: Arc<Activity>, verbose: bool) {
    std::thread::spawn(move || {
        let watched: Arc<Mutex<HashSet<PathBuf>>> = Arc::default();
        let mut warned = false;
        loop {
            let mut opened = 0;
            for (path, device) in evdev::enumerate() {
                opened += 1;
                if watched.lock().unwrap().contains(&path) || !is_input(&device) {
                    continue;
                }
                let id = device.input_id();
                let name = device.name().unwrap_or("").to_string();
                let duck = is_duckwalk_device(id.vendor(), id.product(), &name);
                if verbose {
                    let whose = if duck { "Duckwalk (ignored)" } else { "yours" };
                    println!("device {whose}: {name} ({})", path.display());
                }
                watched.lock().unwrap().insert(path.clone());
                let (activity, watched) = (activity.clone(), watched.clone());
                std::thread::spawn(move || {
                    read(device, duck, &activity);
                    watched.lock().unwrap().remove(&path); // unplugged: look again on the next scan
                });
            }
            if opened > 0 {
                if !activity.is_watching() {
                    activity.user_seen();
                    activity.set_watching();
                }
            } else if !warned {
                warned = true;
                println!(
                    "Can't read any input device. Add yourself to the input group \
                     (sudo usermod -aG input $USER), then log out and in again. \
                     Until then Harold walks on his timer."
                );
            }
            std::thread::sleep(RESCAN);
        }
    });
}

/// Keyboards, mice, touchpads, touch screens and pens; not power buttons'
/// switches or the lid.
fn is_input(device: &Device) -> bool {
    let events = device.supported_events();
    events.contains(EventType::KEY) || events.contains(EventType::RELATIVE) || events.contains(EventType::ABSOLUTE)
}

fn read(mut device: Device, duck: bool, activity: &Activity) {
    while let Ok(events) = device.fetch_events() {
        let any = events.into_iter().any(|e| e.event_type() != EventType::SYNCHRONIZATION);
        if !any {
            continue;
        }
        if duck {
            IGNORED.fetch_add(1, Ordering::Relaxed);
        } else {
            activity.user_seen();
        }
    }
}

pub fn ignored() -> u64 {
    IGNORED.load(Ordering::Relaxed)
}
