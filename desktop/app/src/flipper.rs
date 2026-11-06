//! What Duckwalk tells the app (`app/helper_protocol.h`, Flipper -> PC): its
//! state, each walk as it starts, a quack, and that it's closing. Packets
//! arrive over USB (the vendor report's IN direction) or Bluetooth (a
//! notification), and both end up in `Harold::receive`.

use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::Mutex;

use crate::protocol::{MAGIC, PACKET_LEN, VERSION};

pub const MSG_STATE: u8 = 0x81;
pub const MSG_WALK: u8 = 0x82;
pub const MSG_QUACK: u8 = 0x83;
pub const MSG_CLOSING: u8 = 0x84;

const STATE_AWAKE: u8 = 1 << 0;
const STATE_WALKING: u8 = 1 << 1;
const STATE_WAITING: u8 = 1 << 2;
const WALK_HEADER: usize = 9;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Off,
    Subtle,
    Walk,
    Wander,
    Unknown(u8),
}

impl Mode {
    fn from(b: u8) -> Self {
        match b {
            0 => Mode::Off,
            1 => Mode::Subtle,
            2 => Mode::Walk,
            3 => Mode::Wander,
            n => Mode::Unknown(n),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Message {
    State {
        awake: bool,
        walking: bool,
        waiting: bool,
        mode: Mode,
        bluetooth: bool,
    },
    Walk {
        total: u16,
        first: u16,
        step_ms: u8,
        steps: Vec<(i8, i8)>,
    },
    Quack,
    Closing,
}

/// One packet, or `None` when it isn't a message from Duckwalk this app
/// knows (newer types are skipped, so a newer Flipper build doesn't break it).
pub fn parse(p: &[u8]) -> Option<Message> {
    if p.len() < 3 || p[0] != MAGIC || p[1] != VERSION {
        return None;
    }
    match p[2] {
        MSG_STATE if p.len() >= 6 => Some(Message::State {
            awake: p[3] & STATE_AWAKE != 0,
            walking: p[3] & STATE_WALKING != 0,
            waiting: p[3] & STATE_WAITING != 0,
            mode: Mode::from(p[4]),
            bluetooth: p[5] == 1,
        }),
        MSG_WALK if p.len() >= WALK_HEADER => {
            let n = p[7] as usize;
            if WALK_HEADER + n * 2 > p.len().min(PACKET_LEN) {
                return None;
            }
            let steps = (0..n)
                .map(|i| (p[WALK_HEADER + i * 2] as i8, p[WALK_HEADER + i * 2 + 1] as i8))
                .collect();
            Some(Message::Walk {
                total: u16::from_le_bytes([p[3], p[4]]),
                first: u16::from_le_bytes([p[5], p[6]]),
                step_ms: p[8],
                steps,
            })
        }
        MSG_QUACK => Some(Message::Quack),
        MSG_CLOSING => Some(Message::Closing),
        _ => None,
    }
}

/// A walk put together from its pieces: the pointer's offset from where the
/// walk starts, one point per step.
#[derive(Default)]
pub struct WalkBuilder {
    total: u16,
    step_ms: u8,
    steps: Vec<(i8, i8)>,
}

impl WalkBuilder {
    /// Add a piece; returns the whole walk once the last piece is in. A piece
    /// out of order starts over (a walk the app joined halfway is dropped).
    pub fn add(&mut self, total: u16, first: u16, step_ms: u8, steps: &[(i8, i8)]) -> Option<(u8, Vec<(i32, i32)>)> {
        if first == 0 {
            *self = WalkBuilder {
                total,
                step_ms,
                steps: Vec::with_capacity(total as usize),
            };
        } else if first as usize != self.steps.len() || total != self.total {
            *self = WalkBuilder::default();
            return None;
        }
        self.steps.extend_from_slice(steps);
        if self.total == 0 || self.steps.len() < self.total as usize {
            return None;
        }
        let (mut x, mut y) = (0i32, 0i32);
        let points = self
            .steps
            .iter()
            .map(|&(dx, dy)| {
                x += dx as i32;
                y += dy as i32;
                (x, y)
            })
            .collect();
        let done = (self.step_ms, points);
        *self = WalkBuilder::default();
        Some(done)
    }
}

/// What the app knows about Harold, from his messages.
pub struct Harold {
    heard: AtomicBool,
    awake: AtomicBool,
    walking: AtomicBool,
    waiting: AtomicBool,
    mode: AtomicU8,
    walk: Mutex<WalkBuilder>,
}

pub static HAROLD: Harold = Harold {
    heard: AtomicBool::new(false),
    awake: AtomicBool::new(false),
    walking: AtomicBool::new(false),
    waiting: AtomicBool::new(false),
    mode: AtomicU8::new(0),
    walk: Mutex::new(WalkBuilder {
        total: 0,
        step_ms: 0,
        steps: Vec::new(),
    }),
};

impl Harold {
    /// Take in one packet from the Flipper; `--verbose` logs every message.
    pub fn receive(&self, packet: &[u8], verbose: bool) {
        let Some(msg) = parse(packet) else { return };
        match &msg {
            Message::State {
                awake,
                walking,
                waiting,
                mode,
                ..
            } => {
                self.heard.store(true, Ordering::Relaxed);
                self.awake.store(*awake, Ordering::Relaxed);
                self.walking.store(*walking, Ordering::Relaxed);
                self.waiting.store(*waiting, Ordering::Relaxed);
                self.mode.store(
                    match mode {
                        Mode::Off => 0,
                        Mode::Subtle => 1,
                        Mode::Walk => 2,
                        Mode::Wander => 3,
                        Mode::Unknown(n) => *n,
                    },
                    Ordering::Relaxed,
                );
                if verbose {
                    println!("Harold: {msg:?}");
                }
            }
            Message::Walk {
                total,
                first,
                step_ms,
                steps,
            } => {
                let done = self.walk.lock().unwrap().add(*total, *first, *step_ms, steps);
                if let (true, Some((ms, points))) = (verbose, done) {
                    let far = points.iter().map(|(x, y)| x.abs().max(y.abs())).max().unwrap_or(0);
                    println!(
                        "Harold: a walk of {} steps, {} ms each, up to {far} px away, ending at {:?}",
                        points.len(),
                        ms,
                        points.last().copied().unwrap_or((0, 0))
                    );
                }
            }
            Message::Quack | Message::Closing => {
                if matches!(msg, Message::Closing) {
                    self.heard.store(false, Ordering::Relaxed);
                }
                if verbose {
                    println!("Harold: {msg:?}");
                }
            }
        }
    }

    /// Has Duckwalk talked to this app (a build that talks back)?
    pub fn heard(&self) -> bool {
        self.heard.load(Ordering::Relaxed)
    }

    pub fn awake(&self) -> bool {
        self.awake.load(Ordering::Relaxed)
    }

    pub fn walking(&self) -> bool {
        self.walking.load(Ordering::Relaxed)
    }

    pub fn waiting(&self) -> bool {
        self.waiting.load(Ordering::Relaxed)
    }

    /// The keep-awake mode, by name.
    pub fn mode_name(&self) -> &'static str {
        match self.mode.load(Ordering::Relaxed) {
            0 => "off",
            1 => "subtle",
            2 => "walk",
            3 => "wander",
            _ => "unknown",
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn packet(t: u8, data: &[u8]) -> [u8; PACKET_LEN] {
        let mut p = [0u8; PACKET_LEN];
        p[0] = MAGIC;
        p[1] = VERSION;
        p[2] = t;
        p[3..3 + data.len()].copy_from_slice(data);
        p
    }

    #[test]
    fn state() {
        let m = parse(&packet(MSG_STATE, &[STATE_AWAKE | STATE_WAITING, 2, 1]));
        assert_eq!(
            m,
            Some(Message::State {
                awake: true,
                walking: false,
                waiting: true,
                mode: Mode::Walk,
                bluetooth: true
            })
        );
    }

    #[test]
    fn unknown_and_foreign_packets_are_skipped() {
        assert_eq!(parse(&packet(0x99, &[])), None);
        assert_eq!(parse(&packet(0x01, &[])), None, "the app's own idle packet");
        let mut p = packet(MSG_QUACK, &[]);
        p[0] = b'X';
        assert_eq!(parse(&p), None);
    }

    #[test]
    fn walk_from_pieces() {
        let mut b = WalkBuilder::default();
        // 3 steps: (1,0) (1,-1) (-2,1), in two pieces.
        let p1 = packet(MSG_WALK, &[3, 0, 0, 0, 2, 20, 1, 0, 1, 0xFF]);
        let p2 = packet(MSG_WALK, &[3, 0, 2, 0, 1, 20, 0xFE, 1]);
        let Some(Message::Walk {
            total,
            first,
            step_ms,
            steps,
        }) = parse(&p1)
        else {
            panic!()
        };
        assert_eq!(b.add(total, first, step_ms, &steps), None);
        let Some(Message::Walk {
            total,
            first,
            step_ms,
            steps,
        }) = parse(&p2)
        else {
            panic!()
        };
        assert_eq!(
            b.add(total, first, step_ms, &steps),
            Some((20, vec![(1, 0), (2, -1), (0, 0)]))
        );
    }

    #[test]
    fn a_walk_joined_halfway_is_dropped() {
        let mut b = WalkBuilder::default();
        assert_eq!(b.add(30, 27, 20, &[(1, 1); 3]), None);
        assert!(b.steps.is_empty());
    }
}
