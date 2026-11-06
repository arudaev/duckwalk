//! `duckwalk mcp`: a small MCP server over stdio, so a coding agent (Claude
//! Code, Codex and others) can see Harold's status and wake him, put him to
//! sleep or change his mode. It talks to the running tray app through its
//! control socket (control.rs) and never touches the network.
//!
//! MCP over stdio is JSON-RPC 2.0, one message per line. Only what a tools
//! server needs is here: initialize, ping, tools/list, tools/call.

use std::io::{BufRead, Write};

use serde_json::{json, Value};

use crate::control;

const PROTOCOL: &str = "2025-06-18";

fn tools() -> Value {
    let none = json!({"type": "object", "properties": {}});
    json!([
        {
            "name": "status",
            "description": "Harold's status: whether the Flipper is connected (usb, bluetooth or none), whether keep-awake is on, its mode, whether he's walking the pointer right now or waiting because you're working, and how long since your last own input.",
            "inputSchema": none,
        },
        {
            "name": "wake",
            "description": "Wake Harold: keep-awake on, in his usual mode. The same as pressing OK on the Flipper while he sleeps. Use before a long task so the computer doesn't sleep or lock.",
            "inputSchema": none,
        },
        {
            "name": "sleep",
            "description": "Put Harold to sleep: keep-awake off, so the computer can sleep and lock as usual. The same as pressing OK on the Flipper while he's awake.",
            "inputSchema": none,
        },
        {
            "name": "set_mode",
            "description": "Wake Harold in a given keep-awake mode and keep it: subtle (one pixel now and then, invisible), walk (a short walk every few minutes), wander (the pointer drifts without stopping).",
            "inputSchema": {
                "type": "object",
                "properties": {"mode": {"type": "string", "enum": control::MODES}},
                "required": ["mode"],
            },
        },
    ])
}

fn call(name: &str, args: &Value) -> (String, bool) {
    let req = match name {
        "status" => json!({"cmd": "status"}),
        "wake" => json!({"cmd": "wake"}),
        "sleep" => json!({"cmd": "sleep"}),
        "set_mode" => json!({"cmd": "mode", "mode": args["mode"]}),
        other => return (format!("Unknown tool {other}"), true),
    };
    match control::request(&req) {
        Ok(answer) => {
            let failed = answer["ok"] == json!(false);
            (serde_json::to_string_pretty(&answer).unwrap_or_default(), failed)
        }
        Err(e) => (e, true),
    }
}

fn answer(msg: &Value) -> Option<Value> {
    let id = msg.get("id")?.clone(); // notifications get no answer
    let result = match msg["method"].as_str().unwrap_or("") {
        "initialize" => json!({
            "protocolVersion": msg["params"]["protocolVersion"].as_str().unwrap_or(PROTOCOL),
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "duckwalk", "version": env!("CARGO_PKG_VERSION")},
            "instructions": "Duckwalk keeps this computer awake through Harold, a rubber duck on a Flipper Zero. Check status first; wake him before long unattended work and put him to sleep afterwards.",
        }),
        "ping" => json!({}),
        "tools/list" => json!({"tools": tools()}),
        "tools/call" => {
            let (text, is_error) = call(
                msg["params"]["name"].as_str().unwrap_or(""),
                &msg["params"]["arguments"],
            );
            json!({"content": [{"type": "text", "text": text}], "isError": is_error})
        }
        method => {
            return Some(
                json!({"jsonrpc": "2.0", "id": id, "error": {"code": -32601, "message": format!("no method {method}")}}),
            );
        }
    };
    Some(json!({"jsonrpc": "2.0", "id": id, "result": result}))
}

pub fn run() {
    let stdin = std::io::stdin();
    let mut stdout = std::io::stdout();
    for line in stdin.lock().lines().map_while(Result::ok) {
        if line.trim().is_empty() {
            continue;
        }
        let reply = match serde_json::from_str::<Value>(&line) {
            Ok(msg) => answer(&msg),
            Err(e) => Some(json!({"jsonrpc": "2.0", "id": null, "error": {"code": -32700, "message": e.to_string()}})),
        };
        if let Some(reply) = reply {
            let _ = writeln!(stdout, "{reply}");
            let _ = stdout.flush();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn handshake_and_list() {
        let init = answer(
            &json!({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-06-18"}}),
        )
        .unwrap();
        assert_eq!(init["result"]["serverInfo"]["name"], "duckwalk");
        assert!(answer(&json!({"jsonrpc": "2.0", "method": "notifications/initialized"})).is_none());
        let list = answer(&json!({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})).unwrap();
        let names: Vec<_> = list["result"]["tools"]
            .as_array()
            .unwrap()
            .iter()
            .map(|t| t["name"].as_str().unwrap())
            .collect();
        assert_eq!(names, ["status", "wake", "sleep", "set_mode"]);
        let bad = answer(&json!({"jsonrpc": "2.0", "id": 3, "method": "resources/list"})).unwrap();
        assert_eq!(bad["error"]["code"], -32601);
    }
}
