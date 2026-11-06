//! Duckwalk's desktop app: it watches for your own input (not Duckwalk's) and
//! tells Harold over USB or Bluetooth how long you have been idle, so he
//! walks the pointer only while you are away. It lives in the tray, starts at
//! login, and says goodbye to the Flipper when you quit.

// No console window on Windows; `--verbose` from a terminal still prints.
#![cfg_attr(windows, windows_subsystem = "windows")]

mod ble;
mod flipper;
mod input;
mod install;
mod login;
mod protocol;
mod status;
mod tray;
mod usb;

use std::sync::mpsc;
use std::sync::Arc;
use std::time::{Duration, Instant};

use tokio::sync::{watch, Notify};

use protocol::{bye_packet, idle_packet, usb_report, HEARTBEAT};
use status::{Connection, STATUS};

const RETRY_OPEN: Duration = Duration::from_secs(2);

const USAGE: &str = "\
Usage: duckwalk [--verbose] [--pretend-working] [--no-install]

Duckwalk's desktop app. Tells Harold over USB or Bluetooth when you use
your own mouse or keyboard, so he walks the pointer only while you are
away. It lives in the tray and starts at login.

  --verbose          print what it finds and sends
  --pretend-working  say you are working all the time, to check the link:
                     the Flipper should show \"You're working\"
  --no-install       run from here; don't copy to the install folder
                     (Windows) or point start at login at this copy";

/// When you are active, tell Harold straight away, but at most this often.
const SEND_ACTIVE: Duration = Duration::from_millis(250);

/// How often the app reads what Duckwalk sent over USB.
const LISTEN: Duration = Duration::from_millis(50);

/// How long Quit waits for the goodbye to reach the Flipper.
const BYE_WAIT: Duration = Duration::from_millis(600);

fn user_idle(activity: &input::Activity, pretend_working: bool) -> Duration {
    if pretend_working {
        Duration::ZERO
    } else {
        activity.idle()
    }
}

/// Started from a terminal, print there; started at login, print nowhere.
#[cfg(windows)]
fn attach_console() {
    use windows::Win32::System::Console::{AttachConsole, ATTACH_PARENT_PROCESS};
    let _ = unsafe { AttachConsole(ATTACH_PARENT_PROCESS) };
}
#[cfg(not(windows))]
fn attach_console() {}

const INSTANCE: &str = "duckwalk-desktop-app";

/// No other Duckwalk is running. The check lets go at once, so a copy this
/// one starts after installing can take over.
fn only_one() -> bool {
    !single_instance::SingleInstance::new(INSTANCE).is_ok_and(|i| !i.is_single())
}

fn main() {
    attach_console();
    let mut verbose = false;
    let mut pretend_working = false;
    let mut no_install = false;
    for arg in std::env::args().skip(1) {
        match arg.as_str() {
            "--verbose" | "-v" => verbose = true,
            "--pretend-working" => pretend_working = true,
            "--no-install" => no_install = true,
            _ => {
                println!("{USAGE}");
                return;
            }
        }
    }

    // One Duckwalk at a time: login and a double-click shouldn't make two.
    if !only_one() {
        install::tell(
            "Duckwalk is already running: Harold is in the tray.

             To update it, quit it from his menu, then open the new download again.",
        );
        return;
    }
    // Started from a download: move to where it can stay, and start from there.
    let installed = match install::ensure(no_install) {
        install::Installed::Moved => return,
        install::Installed::Here(installed) => installed,
    };
    let instance = single_instance::SingleInstance::new(INSTANCE);
    if instance.as_ref().is_ok_and(|i| !i.is_single()) {
        return;
    }
    if installed && login::is_enabled() {
        login::set(true); // point start at login at this copy
    }

    println!(
        "Duckwalk desktop app {}: looking for Harold over USB and Bluetooth.",
        env!("CARGO_PKG_VERSION")
    );
    login::first_run();

    let quit = Arc::new(Notify::new());
    let (done_tx, done_rx) = mpsc::channel::<()>();
    let start = {
        let quit = quit.clone();
        move || {
            std::thread::spawn(move || {
                let runtime = tokio::runtime::Runtime::new().expect("tokio runtime");
                runtime.block_on(link(verbose, pretend_working, quit));
                let _ = done_tx.send(());
            });
        }
    };

    tray::run(start, move || {
        quit.notify_one();
        let _ = done_rx.recv_timeout(BYE_WAIT * 2);
        let _ = &instance; // held until the very end
    });
}

/// Keep Harold told: an idle packet every second and straight away when you
/// are active, over USB when he's plugged in, otherwise over Bluetooth. On
/// `quit`, say goodbye and return.
async fn link(verbose: bool, pretend_working: bool, quit: Arc<Notify>) {
    let activity = input::Activity::new();
    input::start(activity.clone(), verbose);

    let (packet_tx, packet_rx) = watch::channel(idle_packet(user_idle(&activity, pretend_working)));
    let (usb_busy_tx, usb_busy_rx) = watch::channel(false);
    tokio::spawn(ble::run(packet_rx, usb_busy_rx, verbose));

    let mut usb = usb::Usb::new();
    let mut last_try = Instant::now() - RETRY_OPEN;
    let mut ticker = tokio::time::interval(HEARTBEAT);
    let mut sent = Instant::now() - SEND_ACTIVE;
    // Duckwalk's own packets (flipper.rs) are read this often over USB.
    let mut listen = tokio::time::interval(LISTEN);
    loop {
        tokio::select! {
            _ = listen.tick() => {
                usb.poll(|p| flipper::HAROLD.receive(p, verbose));
                continue;
            }
            _ = ticker.tick() => {}
            _ = activity.seen() => {
                tokio::time::sleep(SEND_ACTIVE.saturating_sub(sent.elapsed())).await;
            }
            _ = quit.notified() => {
                let bye = bye_packet();
                if usb.is_open() {
                    usb.send(&usb_report(&bye));
                }
                packet_tx.send_replace(bye);
                tokio::time::sleep(BYE_WAIT).await; // for Bluetooth to write it
                println!("Goodbye.");
                return;
            }
        }
        sent = Instant::now();
        let idle = user_idle(&activity, pretend_working);
        let packet = idle_packet(idle);
        packet_tx.send_replace(packet);

        if !usb.is_open() && last_try.elapsed() >= RETRY_OPEN {
            last_try = Instant::now();
            if usb.open(verbose) {
                println!("Connected to Duckwalk over USB.");
            }
        }
        if usb.is_open() && !usb.send(&usb_report(&packet)) {
            println!("Lost Duckwalk over USB (unplugged, or closed). Waiting for it...");
        }
        usb_busy_tx.send_if_modified(|busy| {
            let changed = *busy != usb.is_open();
            *busy = usb.is_open();
            changed
        });

        STATUS.set_connection(if usb.is_open() {
            Connection::Usb
        } else if ble::LINKED.load(std::sync::atomic::Ordering::Relaxed) {
            Connection::Bluetooth
        } else {
            Connection::None
        });
        STATUS.set_asleep(flipper::HAROLD.heard() && !flipper::HAROLD.awake());
        STATUS.set_idle_s(if activity.is_watching() || pretend_working {
            idle.as_secs().min(u32::MAX as u64 - 1) as u32
        } else {
            status::UNWATCHED
        });

        if verbose {
            println!(
                "idle {:.1} s, {} of Duckwalk's own moves ignored",
                idle.as_secs_f64().min(u32::MAX as f64 / 1000.0),
                input::ignored()
            );
        }
    }
}
