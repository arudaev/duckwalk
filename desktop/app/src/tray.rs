//! Harold in the tray (the menu bar on macOS): the icon is lit while he's
//! connected and grey while the app looks for him, and the menu says how he
//! is connected and whether you're working.

use std::time::Duration;

use tao::event::{Event, StartCause};
use tao::event_loop::{ControlFlow, EventLoopBuilder};
use tray_icon::menu::{CheckMenuItem, Menu, MenuEvent, MenuItem, PredefinedMenuItem};
use tray_icon::{Icon, TrayIcon, TrayIconBuilder};

use crate::login;
use crate::status::{Connection, STATUS};

const HAROLD_LIT: &[u8] = include_bytes!("../assets/harold-32.png");
const HAROLD_GREY: &[u8] = include_bytes!("../assets/harold-32-grey.png");
const REFRESH: Duration = Duration::from_millis(500);

/// What wakes the event loop: a menu click, or the refresh tick.
enum Wake {
    Menu(MenuEvent),
    Tick,
}

/// Harold in colour (brand/tools/make_icon.py), or grey while the app looks
/// for him: yellow reads on light and dark taskbars alike.
fn harold_icon(png_bytes: &[u8]) -> Icon {
    let decoder = png::Decoder::new(std::io::Cursor::new(png_bytes));
    let mut reader = decoder.read_info().expect("Harold's icon");
    let mut buf = vec![0; reader.output_buffer_size().expect("icon size")];
    let info = reader.next_frame(&mut buf).expect("Harold's icon");
    buf.truncate(info.buffer_size());
    Icon::from_rgba(buf, info.width, info.height).expect("tray icon")
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
    let proxy = event_loop.create_proxy();
    std::thread::spawn(move || {
        while proxy.send_event(Wake::Tick).is_ok() {
            std::thread::sleep(REFRESH);
        }
    });

    let state = MenuItem::new(STATUS.line(), false, None);
    let login_item = CheckMenuItem::new("Start at login", true, login::is_enabled(), None);
    let quit_item = MenuItem::new("Quit Duckwalk", true, None);
    let menu = Menu::new();
    let _ = menu.append_items(&[&state, &PredefinedMenuItem::separator(), &login_item, &quit_item]);

    let lit = harold_icon(HAROLD_LIT);
    let grey = harold_icon(HAROLD_GREY);
    let mut tray: Option<TrayIcon> = None;
    let mut shown = (String::new(), None::<bool>);

    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Wait;

        // The tray icon is made once the event loop runs (macOS needs that).
        if let Event::NewEvents(StartCause::Init) = event {
            tray = TrayIconBuilder::new()
                .with_menu(Box::new(menu.clone()))
                .with_tooltip("Duckwalk")
                .with_icon(grey.clone())
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
            let connected = STATUS.connection() != Connection::None;
            if shown.0 != line {
                state.set_text(&line);
                let _ = tray.set_tooltip(Some(format!("Duckwalk: {line}")));
                shown.0 = line;
            }
            if shown.1 != Some(connected) {
                let _ = tray.set_icon(Some(if connected { lit.clone() } else { grey.clone() }));
                shown.1 = Some(connected);
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
