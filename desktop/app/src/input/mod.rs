//! Seeing your input: when did you last touch your own mouse, keyboard,
//! touchpad, touch screen or pen? Duckwalk's own devices never count.
//!
//! Each system has its own way to tell devices apart (decision record 0002):
//! Raw Input on Windows, evdev on Linux, IOHIDManager on macOS. Each one
//! calls `Activity::user_seen` from a thread of its own.

use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use tokio::sync::Notify;

#[cfg(windows)]
mod windows;
#[cfg(windows)]
pub use self::windows::{ignored, start};

#[cfg(not(windows))]
mod unsupported;
#[cfg(not(windows))]
pub use self::unsupported::{ignored, start};

/// Shared between the input thread and the sender.
pub struct Activity {
    started: Instant,
    /// Milliseconds after `started` of the user's last own input.
    last_ms: AtomicU64,
    /// True once input is being watched; until then the app reports "away".
    watching: AtomicBool,
    seen: Notify,
}

impl Activity {
    pub fn new() -> Arc<Self> {
        Arc::new(Activity {
            started: Instant::now(),
            last_ms: AtomicU64::new(0),
            watching: AtomicBool::new(false),
            seen: Notify::new(),
        })
    }

    #[cfg_attr(not(windows), allow(dead_code))]
    pub fn set_watching(&self) {
        self.watching.store(true, Ordering::Relaxed);
    }

    pub fn is_watching(&self) -> bool {
        self.watching.load(Ordering::Relaxed)
    }

    /// You did something.
    #[cfg_attr(not(windows), allow(dead_code))]
    pub fn user_seen(&self) {
        let now = self.started.elapsed().as_millis() as u64;
        self.last_ms.store(now, Ordering::Relaxed);
        self.seen.notify_one();
    }

    /// How long since your last own input; "away" while nothing is watched.
    pub fn idle(&self) -> Duration {
        if !self.is_watching() {
            return Duration::from_millis(u32::MAX as u64);
        }
        let now = self.started.elapsed().as_millis() as u64;
        Duration::from_millis(now.saturating_sub(self.last_ms.load(Ordering::Relaxed)))
    }

    /// Wait for the next `user_seen`.
    pub async fn seen(&self) {
        self.seen.notified().await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn away_until_watching() {
        let a = Activity::new();
        assert_eq!(a.idle(), Duration::from_millis(u32::MAX as u64));
        a.set_watching();
        a.user_seen();
        assert!(a.idle() < Duration::from_secs(1));
    }
}
