//! macOS and Linux: not watched yet (evdev and IOHIDManager follow), so the
//! app reports "away" and Harold walks as he would without it.

use std::sync::Arc;

use super::Activity;

pub fn start(_activity: Arc<Activity>, _verbose: bool) {
    println!("Seeing your input isn't ready on this system yet; Harold walks on his timer.");
}

pub fn ignored() -> u64 {
    0
}
