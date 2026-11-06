//! The tray app's control socket: how `duckwalk mcp` (and anything else on
//! this computer) asks for Harold's status or sends him a command. A local
//! socket, never a network port: a named pipe on Windows, a Unix socket
//! elsewhere. One JSON request per connection, one JSON line back:
//!
//!   {"cmd": "status"}
//!   {"cmd": "wake"} | {"cmd": "sleep"} | {"cmd": "mode", "mode": "wander"}

use std::io::{BufRead, BufReader, Write};

use interprocess::local_socket::{prelude::*, GenericNamespaced, ListenerOptions, Name, Stream};
use serde_json::{json, Value};

use crate::flipper::HAROLD;
use crate::status::{Connection, STATUS, UNWATCHED};

const NAME: &str = "duckwalk-control";

/// A command for Harold, as the Flipper takes it (`HELPER_CMD_*`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Command {
    Wake,
    Sleep,
    Mode(u8),
}

pub const MODES: [&str; 3] = ["subtle", "walk", "wander"];

fn name() -> std::io::Result<Name<'static>> {
    NAME.to_ns_name::<GenericNamespaced>()
}

/// Harold's status, for an agent: what he's doing and how he's connected.
pub fn status() -> Value {
    let connection = match STATUS.connection() {
        Connection::None => "none",
        Connection::Usb => "usb",
        Connection::Bluetooth => "bluetooth",
    };
    let idle = STATUS.idle_s();
    json!({
        "running": true,
        "connection": connection,
        "harold": if HAROLD.heard() {
            json!({
                "awake": HAROLD.awake(),
                "mode": HAROLD.mode_name(),
                "walking": HAROLD.walking(),
                "waiting_while_you_work": HAROLD.waiting(),
            })
        } else {
            Value::Null
        },
        "your_idle_seconds": if idle == UNWATCHED { Value::Null } else { json!(idle) },
    })
}

/// Turn a request into a command, or an answer of its own.
pub fn parse(request: &Value) -> Result<Option<Command>, String> {
    match request["cmd"].as_str() {
        Some("status") => Ok(None),
        Some("wake") => Ok(Some(Command::Wake)),
        Some("sleep") => Ok(Some(Command::Sleep)),
        Some("mode") => {
            let mode = request["mode"].as_str().unwrap_or("");
            match MODES.iter().position(|m| *m == mode) {
                Some(i) => Ok(Some(Command::Mode(i as u8 + 1))),
                None => Err(format!("unknown mode {mode:?}; use subtle, walk or wander")),
            }
        }
        other => Err(format!("unknown request {other:?}")),
    }
}

/// Serve requests on a thread of its own. `send` hands a command to the link
/// and says how it went to the Flipper, or why it couldn't.
pub fn serve(send: impl Fn(Command) -> Result<&'static str, String> + Send + 'static) {
    std::thread::spawn(move || {
        let Ok(listener) = name().and_then(|n| ListenerOptions::new().name(n).create_sync()) else {
            println!("Can't open the control socket; agents can't reach Duckwalk.");
            return;
        };
        for conn in listener.incoming().flatten() {
            let mut reader = BufReader::new(conn);
            let mut line = String::new();
            if reader.read_line(&mut line).is_err() {
                continue;
            }
            let answer = match serde_json::from_str::<Value>(&line)
                .map_err(|e| e.to_string())
                .and_then(|r| parse(&r))
            {
                Ok(None) => status(),
                Ok(Some(cmd)) => match send(cmd) {
                    Ok(via) => json!({"ok": true, "sent_over": via}),
                    Err(e) => json!({"ok": false, "error": e}),
                },
                Err(e) => json!({"ok": false, "error": e}),
            };
            let _ = writeln!(reader.get_mut(), "{answer}");
        }
    });
}

/// Ask the running tray app. Fails when it isn't running.
pub fn request(req: &Value) -> Result<Value, String> {
    let stream = name()
        .and_then(Stream::connect)
        .map_err(|_| "The Duckwalk desktop app isn't running. Start it first (it lives in the tray).".to_string())?;
    let mut reader = BufReader::new(stream);
    writeln!(reader.get_mut(), "{req}").map_err(|e| e.to_string())?;
    let mut line = String::new();
    reader.read_line(&mut line).map_err(|e| e.to_string())?;
    serde_json::from_str(&line).map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn requests() {
        assert_eq!(parse(&json!({"cmd": "status"})), Ok(None));
        assert_eq!(parse(&json!({"cmd": "wake"})), Ok(Some(Command::Wake)));
        assert_eq!(
            parse(&json!({"cmd": "mode", "mode": "wander"})),
            Ok(Some(Command::Mode(3)))
        );
        assert!(parse(&json!({"cmd": "mode", "mode": "jog"})).is_err());
        assert!(parse(&json!({"cmd": "dance"})).is_err());
    }
}
