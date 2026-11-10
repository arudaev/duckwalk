//! Where the pointer is and where Harold may take it (`HELPER_MSG_AREA`):
//! the safe area is the middle of the active window, clear of its title bar
//! and edges and never over the taskbar, and the screen is the one the
//! pointer is on. Wander keeps to the safe area, a walk to the screen, and
//! both land on the pixel they set off from.
//!
//! Windows only for now; elsewhere the app sends no area and Harold wanders
//! as before.

// Only Windows reports an area so far.
#![cfg_attr(not(windows), allow(dead_code))]

use crate::protocol::{MAGIC, PACKET_LEN, VERSION};

pub const MSG_AREA: u8 = 0x04;

/// A rectangle in pixels, the right and bottom edges just outside.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Rect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl Rect {
    fn width(&self) -> i32 {
        self.right - self.left
    }

    fn height(&self) -> i32 {
        self.bottom - self.top
    }

    fn intersect(&self, o: &Rect) -> Rect {
        Rect {
            left: self.left.max(o.left),
            top: self.top.max(o.top),
            right: self.right.min(o.right),
            bottom: self.bottom.min(o.bottom),
        }
    }

    /// Smaller by a fifth of its size on every side, but at least `MARGIN`,
    /// and never smaller than `SMALLEST` across.
    fn middle(&self) -> Rect {
        let (w, h) = (self.width(), self.height());
        let dx = (w / 5).max(MARGIN).min((w - SMALLEST).max(0) / 2);
        let dy = (h / 5).max(MARGIN).min((h - SMALLEST).max(0) / 2);
        Rect {
            left: self.left + dx,
            top: self.top + dy,
            right: self.right - dx,
            bottom: self.bottom - dy,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Area {
    pub x: i32,
    pub y: i32,
    pub safe: Rect,
    pub screen: Rect,
}

/// Room the wander keeps from an edge at least.
const MARGIN: i32 = 24;
/// The title bar, tabs and toolbars: kept clear at the top of a window.
const TITLE_BAR: i32 = 48;
/// A safe area is never narrower or shorter than this.
const SMALLEST: i32 = 40;
/// A window smaller than this (a tooltip, a dialog's corner) isn't worth
/// wandering in: the work area is used instead.
const SMALL_WINDOW: i32 = 160;

/// The safe area: the middle of `window` as far as it lies in `work` (the
/// screen without the taskbar), or the middle of `work` when there is no
/// window worth wandering in.
pub fn safe_area(window: Option<Rect>, work: Rect) -> Rect {
    let inside = window.map(|w| w.intersect(&work)).map(|mut r| {
        r.top = (r.top + TITLE_BAR).min(r.bottom);
        r
    });
    match inside {
        Some(r) if r.width() >= SMALL_WINDOW && r.height() >= SMALL_WINDOW - TITLE_BAR => r.middle(),
        _ => work.middle(),
    }
}

fn put_i16(p: &mut [u8], v: i32) {
    let v = v.clamp(i16::MIN as i32, i16::MAX as i32) as i16;
    p[..2].copy_from_slice(&v.to_le_bytes());
}

fn put_rect(p: &mut [u8], r: &Rect) {
    put_i16(&mut p[0..], r.left);
    put_i16(&mut p[2..], r.top);
    put_i16(&mut p[4..], r.right);
    put_i16(&mut p[6..], r.bottom);
}

/// The area packet: pointer, safe area, screen, each signed 16-bit.
pub fn packet(a: &Area) -> [u8; PACKET_LEN] {
    let mut p = [0u8; PACKET_LEN];
    p[0] = MAGIC;
    p[1] = VERSION;
    p[2] = MSG_AREA;
    put_i16(&mut p[3..], a.x);
    put_i16(&mut p[5..], a.y);
    put_rect(&mut p[7..], &a.safe);
    put_rect(&mut p[15..], &a.screen);
    p
}

/// Where the pointer is now, and where Harold may take it.
#[cfg(windows)]
pub fn current() -> Option<Area> {
    use windows::Win32::Foundation::{POINT, RECT};
    use windows::Win32::Graphics::Gdi::{
        GetMonitorInfoW, MonitorFromPoint, MonitorFromWindow, HMONITOR, MONITORINFO, MONITOR_DEFAULTTONEAREST,
    };
    use windows::Win32::UI::WindowsAndMessaging::{
        GetClassNameW, GetCursorPos, GetForegroundWindow, GetWindowRect, IsIconic, IsWindowVisible,
    };

    fn rect(r: RECT) -> Rect {
        Rect {
            left: r.left,
            top: r.top,
            right: r.right,
            bottom: r.bottom,
        }
    }

    /// The monitor's whole screen and its work area (without the taskbar).
    fn monitor(m: HMONITOR) -> Option<(Rect, Rect)> {
        let mut info = MONITORINFO {
            cbSize: std::mem::size_of::<MONITORINFO>() as u32,
            ..Default::default()
        };
        unsafe { GetMonitorInfoW(m, &mut info) }
            .as_bool()
            .then(|| (rect(info.rcMonitor), rect(info.rcWork)))
    }

    let mut pt = POINT::default();
    unsafe { GetCursorPos(&mut pt) }.ok()?;
    let (screen, pointer_work) = monitor(unsafe { MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST) })?;

    // The active window, unless it is the desktop or the taskbar, hidden or
    // minimised: then the work area of the pointer's screen.
    let fg = unsafe { GetForegroundWindow() };
    let mut window = None;
    let mut work = pointer_work;
    if !fg.is_invalid() && unsafe { IsWindowVisible(fg) }.as_bool() && !unsafe { IsIconic(fg) }.as_bool() {
        let mut class = [0u16; 64];
        let n = unsafe { GetClassNameW(fg, &mut class) } as usize;
        let class = String::from_utf16_lossy(&class[..n]);
        let shell = matches!(
            class.as_str(),
            "Progman" | "WorkerW" | "Shell_TrayWnd" | "Shell_SecondaryTrayWnd"
        );
        let mut r = RECT::default();
        if !shell && unsafe { GetWindowRect(fg, &mut r) }.is_ok() {
            if let Some((_, w)) = monitor(unsafe { MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST) }) {
                work = w;
            }
            window = Some(rect(r));
        }
    }
    Some(Area {
        x: pt.x,
        y: pt.y,
        safe: safe_area(window, work),
        screen,
    })
}

#[cfg(not(windows))]
pub fn current() -> Option<Area> {
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    const WORK: Rect = Rect {
        left: 0,
        top: 0,
        right: 1920,
        bottom: 1032,
    };

    #[test]
    fn a_maximised_window_gives_its_middle_above_the_taskbar() {
        let window = Rect {
            left: -8,
            top: -8,
            right: 1928,
            bottom: 1040,
        };
        let s = safe_area(Some(window), WORK);
        assert_eq!(
            s,
            Rect {
                left: 384,
                top: 244,
                right: 1536,
                bottom: 836
            }
        );
        assert!(s.bottom < WORK.bottom);
    }

    #[test]
    fn a_window_over_the_taskbar_is_cut_at_the_work_area() {
        let window = Rect {
            left: 200,
            top: 600,
            right: 1000,
            bottom: 1200,
        };
        let s = safe_area(Some(window), WORK);
        assert!(s.bottom <= WORK.bottom - MARGIN, "{s:?}");
        assert!(s.top >= 600 + TITLE_BAR, "{s:?}");
    }

    #[test]
    fn a_tiny_window_or_none_gives_the_middle_of_the_screen() {
        let tiny = Rect {
            left: 500,
            top: 500,
            right: 600,
            bottom: 560,
        };
        assert_eq!(safe_area(Some(tiny), WORK), safe_area(None, WORK));
        let s = safe_area(None, WORK);
        assert_eq!(
            s,
            Rect {
                left: 384,
                top: 206,
                right: 1536,
                bottom: 826
            }
        );
    }

    #[test]
    fn a_small_window_still_leaves_room() {
        let small = Rect {
            left: 100,
            top: 100,
            right: 300,
            bottom: 260,
        };
        let s = safe_area(Some(small), WORK);
        assert!(s.width() >= SMALLEST && s.height() >= SMALLEST, "{s:?}");
        assert!(
            s.left > 100 && s.right < 300 && s.top >= 100 + TITLE_BAR && s.bottom < 260,
            "{s:?}"
        );
    }

    #[test]
    fn packet_matches_the_c_helper() {
        let p = packet(&Area {
            x: -5,
            y: 700,
            safe: Rect {
                left: -300,
                top: 200,
                right: 900,
                bottom: 800,
            },
            screen: Rect {
                left: -1920,
                top: 0,
                right: 0,
                bottom: 1080,
            },
        });
        assert_eq!(&p[..3], &[b'D', 1, 0x04]);
        let v: Vec<i16> = (0..10)
            .map(|i| i16::from_le_bytes([p[3 + i * 2], p[4 + i * 2]]))
            .collect();
        assert_eq!(v, [-5, 700, -300, 200, 900, 800, -1920, 0, 0, 1080]);
        assert!(p[23..].iter().all(|&b| b == 0));
    }
}
