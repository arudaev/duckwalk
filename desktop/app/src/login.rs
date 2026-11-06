//! Start at login: the Run key for the current user on Windows, a Launch
//! Agent on macOS, an XDG autostart entry on Linux. On by default: the first
//! time the app runs it turns it on, and after that it's the tray menu's
//! choice.

use auto_launch::{AutoLaunch, AutoLaunchBuilder, LinuxLaunchMode, MacOSLaunchMode, WindowsEnableMode};

fn entry() -> Option<AutoLaunch> {
    let exe = std::env::current_exe().ok()?;
    AutoLaunchBuilder::new()
        .set_app_name("Duckwalk")
        .set_app_path(exe.to_str()?)
        .set_macos_launch_mode(MacOSLaunchMode::LaunchAgent)
        .set_windows_enable_mode(WindowsEnableMode::CurrentUser)
        .set_linux_launch_mode(LinuxLaunchMode::XdgAutostart)
        .build()
        .ok()
}

pub fn is_enabled() -> bool {
    entry().and_then(|e| e.is_enabled().ok()).unwrap_or(false)
}

pub fn set(on: bool) -> bool {
    let Some(e) = entry() else { return false };
    if on {
        e.enable().is_ok()
    } else {
        e.disable().is_ok()
    }
}

/// The first run turns start at login on; a marker file remembers that it
/// did, so turning it off in the menu sticks.
pub fn first_run() {
    let Some(dir) = dirs::config_dir().map(|d| d.join("Duckwalk")) else {
        return;
    };
    let marker = dir.join("first-run-done");
    if marker.exists() {
        return;
    }
    if set(true) {
        println!("Duckwalk now starts at login (the tray menu turns it off).");
    }
    let _ = std::fs::create_dir_all(&dir);
    let _ = std::fs::write(marker, "");
}
