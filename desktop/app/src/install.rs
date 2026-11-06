//! Windows: the app puts itself where it can stay. Started from a download, it
//! copies itself to `%LOCALAPPDATA%\Programs\Duckwalk\duckwalk.exe`, adds a
//! "Duckwalk" shortcut to the Start menu (so Windows Search finds it) and
//! starts that copy; the download can then go. A newer download replaces the
//! installed copy the same way. `--no-install` skips all of it, for running
//! from source.
//!
//! macOS and Linux run from wherever they were started, for now (#127).

pub enum Installed {
    /// Carry on: this is the installed copy (`true`), or installing doesn't
    /// apply here (`false`).
    Here(bool),
    /// The installed copy was started; this process should end (Windows).
    #[cfg_attr(not(windows), allow(dead_code))]
    Moved,
}

#[cfg(not(windows))]
pub fn ensure(_skip: bool) -> Installed {
    Installed::Here(false)
}

/// A message the user sees even without a console.
#[cfg(not(windows))]
pub fn tell(text: &str) {
    println!("{text}");
}

#[cfg(windows)]
pub use self::windows::{ensure, tell};

#[cfg(windows)]
mod windows {
    use std::path::{Path, PathBuf};

    use windows::core::{w, Interface, HSTRING};
    use windows::Win32::System::Com::{
        CoCreateInstance, CoInitializeEx, IPersistFile, CLSCTX_INPROC_SERVER, COINIT_APARTMENTTHREADED,
    };
    use windows::Win32::UI::Shell::{IShellLinkW, ShellLink};
    use windows::Win32::UI::WindowsAndMessaging::{MessageBoxW, MB_ICONINFORMATION, MB_OK};

    use super::Installed;

    fn install_dir() -> Option<PathBuf> {
        Some(dirs::data_local_dir()?.join("Programs").join("Duckwalk"))
    }

    fn start_menu_link() -> Option<PathBuf> {
        Some(dirs::data_dir()?.join(r"Microsoft\Windows\Start Menu\Programs\Duckwalk.lnk"))
    }

    fn same_file(a: &Path, b: &Path) -> bool {
        match (std::fs::canonicalize(a), std::fs::canonicalize(b)) {
            (Ok(a), Ok(b)) => a == b,
            _ => false,
        }
    }

    fn same_content(a: &Path, b: &Path) -> bool {
        match (std::fs::read(a), std::fs::read(b)) {
            (Ok(a), Ok(b)) => a == b,
            _ => false,
        }
    }

    pub fn tell(text: &str) {
        let text = HSTRING::from(text);
        unsafe { MessageBoxW(None, &text, w!("Duckwalk"), MB_OK | MB_ICONINFORMATION) };
    }

    /// The Start menu shortcut, made (or remade) through the shell.
    fn shortcut(target: &Path) -> windows::core::Result<()> {
        let Some(link) = start_menu_link() else { return Ok(()) };
        unsafe {
            let _ = CoInitializeEx(None, COINIT_APARTMENTTHREADED);
            let shell: IShellLinkW = CoCreateInstance(&ShellLink, None, CLSCTX_INPROC_SERVER)?;
            shell.SetPath(&HSTRING::from(target.as_os_str()))?;
            shell.SetDescription(w!("Harold lets go of the pointer while you work"))?;
            shell.SetIconLocation(&HSTRING::from(target.as_os_str()), 0)?;
            let file: IPersistFile = shell.cast()?;
            file.Save(&HSTRING::from(link.as_os_str()), true)?;
        }
        Ok(())
    }

    pub fn ensure(skip: bool) -> Installed {
        let (Ok(me), Some(dir)) = (std::env::current_exe(), install_dir()) else {
            return Installed::Here(false);
        };
        let target = dir.join("duckwalk.exe");
        if skip {
            return Installed::Here(false);
        }
        if same_file(&me, &target) {
            if start_menu_link().is_some_and(|l| !l.exists()) {
                let _ = shortcut(&target);
            }
            return Installed::Here(true);
        }

        // From a download: copy, unless the installed copy is this very build.
        if !same_content(&me, &target) {
            let copied = std::fs::create_dir_all(&dir).and_then(|_| std::fs::copy(&me, &target));
            if copied.is_err() {
                tell("Duckwalk couldn't copy itself to its folder. Open the download again to retry.");
                return Installed::Moved;
            }
        }
        let _ = shortcut(&target);
        let args: Vec<String> = std::env::args().skip(1).collect();
        match std::process::Command::new(&target).args(&args).spawn() {
            Ok(_) => Installed::Moved,
            Err(_) => Installed::Here(false),
        }
    }
}
