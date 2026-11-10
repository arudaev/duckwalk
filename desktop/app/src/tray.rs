//! Harold in the tray (the menu bar on macOS): the icon shows what he's doing
//! on the Flipper, and the menu says how he is connected and whether you're
//! working.
//!
//! Calm by design: he stands still while awake or waiting for you, shuts his
//! eyes while asleep, opens his bill for a moment when he quacks, waddles at
//! four frames a second only while he walks the pointer, and goes grey while
//! there's no Flipper. The icon is only set when the frame changes.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use tao::event::{Event, StartCause};
use tao::event_loop::{ControlFlow, EventLoopBuilder};
use tray_icon::menu::{CheckMenuItem, Menu, MenuEvent, MenuItem, PredefinedMenuItem};
use tray_icon::{Icon, TrayIcon, TrayIconBuilder};

use crate::flipper::HAROLD;
use crate::login;
use crate::status::{Connection, STATUS};

/// Harold's tray frames, 32x32 each, left to right (brand/tools/make_tray.py).
const FRAMES_PNG: &[u8] = include_bytes!("../assets/harold-tray.png");
const IDLE: usize = 0;
const WADDLE: usize = 1; // 1 to 4
const ASLEEP: usize = 5;
const QUACK: usize = 6;
const GREY: usize = 7;

const REFRESH: Duration = Duration::from_millis(500);
const WADDLE_STEP: Duration = Duration::from_millis(250);
const QUACK_SHOWN: Duration = Duration::from_millis(900);

/// What wakes the event loop: a menu click, or the refresh tick.
enum Wake {
    Menu(MenuEvent),
    Tick,
}

/// The frames, cut from the strip.
fn harold_frames() -> Vec<Icon> {
    let decoder = png::Decoder::new(std::io::Cursor::new(FRAMES_PNG));
    let mut reader = decoder.read_info().expect("Harold's frames");
    let mut buf = vec![0; reader.output_buffer_size().expect("frames size")];
    let info = reader.next_frame(&mut buf).expect("Harold's frames");
    let (w, h) = (info.width as usize, info.height as usize);
    (0..w / h)
        .map(|i| {
            let mut rgba = Vec::with_capacity(h * h * 4);
            for y in 0..h {
                let row = (y * w + i * h) * 4;
                rgba.extend_from_slice(&buf[row..row + h * 4]);
            }
            Icon::from_rgba(rgba, h as u32, h as u32).expect("tray icon")
        })
        .collect()
}

/// Which frame shows Harold as he is. `step` counts waddle steps.
fn frame(connected: bool, heard: bool, awake: bool, walking: bool, quacking: bool, step: u128) -> usize {
    if !connected {
        GREY
    } else if !heard {
        IDLE // a Duckwalk build that doesn't talk back: all we know is he's there
    } else if quacking {
        QUACK
    } else if !awake {
        ASLEEP
    } else if walking {
        WADDLE + (step % 4) as usize
    } else {
        IDLE
    }
}

/// Run the tray on this (the main) thread until Quit. `ready` runs once the
/// event loop is up; `quit` tells the Flipper goodbye and returns once it's
/// sent.
pub fn run(ready: impl FnOnce() + 'static, quit: impl Fn() + 'static) -> ! {
    let event_loop = EventLoopBuilder::<Wake>::with_user_event().build();
    let mut ready = Some(ready);
    // Menu clicks and the refresh wake the event loop themselves: after the
    // tray menu closes, Windows stops waking it on its own timer, so a polled
    // queue or a timeout would sit unread.
    let proxy = event_loop.create_proxy();
    MenuEvent::set_event_handler(Some(move |e| {
        let _ = proxy.send_event(Wake::Menu(e));
    }));
    // The tick comes faster only while he waddles.
    let fast = Arc::new(AtomicBool::new(false));
    let proxy = event_loop.create_proxy();
    let ticking = fast.clone();
    std::thread::spawn(move || {
        while proxy.send_event(Wake::Tick).is_ok() {
            std::thread::sleep(if ticking.load(Ordering::Relaxed) {
                WADDLE_STEP
            } else {
                REFRESH
            });
        }
    });

    let state = MenuItem::new(STATUS.line(), false, None);
    let login_item = CheckMenuItem::new("Start at login", true, login::is_enabled(), None);
    let quit_item = MenuItem::new("Quit Duckwalk", true, None);
    let menu = Menu::new();
    let _ = menu.append_items(&[&state, &PredefinedMenuItem::separator(), &login_item, &quit_item]);

    let frames = harold_frames();
    let started = Instant::now();
    let mut tray: Option<TrayIcon> = None;
    let mut shown = (String::new(), None::<usize>);

    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;

        // The tray icon is made once the event loop runs (macOS needs that).
        if let Event::NewEvents(StartCause::Init) = event {
            tray = TrayIconBuilder::new()
                .with_menu(Box::new(menu.clone()))
                .with_tooltip("Duckwalk")
                .with_icon(frames[GREY].clone())
                .build()
                .ok();
            // On Windows the event loop registers Raw Input for its own
            // window, and a program gets one registration per device type:
            // the input watcher must register after it, so it starts now.
            if let Some(ready) = ready.take() {
                ready();
            }
        }

        if let Some(tray) = tray.as_ref() {
            let line = STATUS.line();
            let walking = HAROLD.walking() && HAROLD.awake();
            let now = frame(
                STATUS.connection() != Connection::None,
                HAROLD.heard(),
                HAROLD.awake(),
                walking,
                HAROLD.quacked_within(QUACK_SHOWN),
                started.elapsed().as_millis() / WADDLE_STEP.as_millis(),
            );
            fast.store(walking, Ordering::Relaxed);
            if shown.0 != line {
                state.set_text(&line);
                let _ = tray.set_tooltip(Some(format!("Duckwalk: {line}")));
                shown.0 = line;
            }
            if shown.1 != Some(now) {
                let _ = tray.set_icon(Some(frames[now].clone()));
                shown.1 = Some(now);
            }
        }

        if let Event::UserEvent(Wake::Menu(e)) = event {
            if e.id == quit_item.id() {
                quit();
                *control_flow = ControlFlow::Exit;
            } else if e.id == login_item.id() {
                let on = login_item.is_checked();
                if !login::set(on) {
                    login_item.set_checked(!on); // didn't take: show the truth
                }
            }
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frames_follow_harold() {
        assert_eq!(frame(false, true, true, true, true, 0), GREY);
        assert_eq!(frame(true, false, false, false, false, 0), IDLE);
        assert_eq!(frame(true, true, false, false, true, 0), QUACK);
        assert_eq!(frame(true, true, false, false, false, 0), ASLEEP);
        assert_eq!(frame(true, true, true, false, false, 7), IDLE);
        let walk: Vec<_> = (0..5).map(|s| frame(true, true, true, true, false, s)).collect();
        assert_eq!(walk, [1, 2, 3, 4, 1]);
    }

    #[test]
    fn strip_has_every_frame() {
        assert_eq!(harold_frames().len(), GREY + 1);
    }
}
