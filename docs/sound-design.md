# 10 · Sound design

[← Brand](brand.md) · [Docs home](README.md) · [Next: Decisions →](decisions/0001-tell-duckwalk-from-the-user.md)

The production brief for Duckwalk's sounds: what to make, how it should feel, the technical limits, and how to deliver it. Every sound has its own card below, so you can work on one at a time.

## In short

- **12 sounds, 17 files**: the footsteps come in variations, and the tick is optional.
- **Two families.** Harold's sounds are real, in-world sounds: his voice, his feet. The device's sounds are a small family of designed interface sounds (earcons).
- **Tiny speaker:** 100–2500 Hz. No bass, no sparkle. Design for the middle.
- **Short:** most sounds are under half a second.
- **Deliver** mono WAV masters; we convert them for the Flipper.
- **Start with Priority 1:** the quack, the footsteps, wake and sleep, and the two link sounds.

## Start here

1. Read **Who Harold is** and **The two families** (5 minutes).
2. Skim the **cards** and pick the Priority 1 sounds.
3. Read **The speaker** before you start mixing.
4. Send Round 1 sketches (see **Rounds**).
5. Use the **Delivery checklist** for the final files.

## Who Harold is

Harold is a rubber debugging duck who lives on a Flipper Zero. He sits at a tiny desk reading code on his laptop, walks the computer's cursor around so the computer stays awake, and hunts bugs in the code by quacking at them. He is cheeky, a little smug and always busy, never menacing or loud for the sake of it.

**Reference for feel:** Untitled Goose Game and Desktop Goose. Small comic animal, real sounds, dry and close, funny because of timing and restraint. Use them for mood only: don't sample, trace or imitate their sounds.

## The two families

Games and software separate sounds that happen *in the world* from sounds that *talk to the user*. Duckwalk does the same.

| | Family A: Harold | Family B: the device |
|---|---|---|
| **What** | Harold's voice and body, in the world | Short signals about the device's state |
| **Sounds** | Quack, error quack, error spotted, hop, footsteps | Wake, sleep, stopped, link lost, link back, error fixed, tick |
| **Style** | Real and organic: a duck's voice, small webbed feet on a desk | Designed earcons: one instrument and one motif for the whole family |
| **Rule** | Every sound is something Harold physically does | Same timbre and key for all; meaning comes from the shape |

**Family B as a family.** Pick one instrument that suits a small duck at a desk (a bell, a marimba, a kalimba, a toy piano – your call) and one short motif. Then let the shape carry the meaning:

- Up means good or awake; down means bad or asleep.
- Opposites are mirror images: Wake and Sleep, Link back and Link lost.
- Everything in one key, so any two sounds can follow each other without clashing.

A quack can sit on top of a device sound if it helps (Harold reacting to his own wake-up chime, for example), but the device sounds must work without one.

## How sounds play

Settings → Sound has three levels:

- **Off:** silent.
- **Cues:** only Family B, the sounds that tell you something changed.
- **Lively:** everything, including Harold's sounds.

Harold's sounds play only while he is on screen.

## The cards

Levels are relative to the loudest sound (Link lost, 0 dB). Lengths are targets, not hard limits.

### Family A: Harold

#### A1 · Quack – Priority 1

| | |
|---|---|
| **When** | You hold OK. Harold quacks back at you. |
| **Plays** | Whenever someone presses it, often several times in a row |
| **Intent** | Harold's voice: cheeky, a little nasal, instantly "duck" |
| **Picture it** | A small duck answering back without looking up from his laptop |
| **Length** | 150–400 ms |
| **Level** | −3 dB |
| **Avoid** | A bicycle horn, a squeaky toy, a goose honk, a long dramatic quack |

This is the sound people will remember. Two or three variations are welcome, so repeated presses don't sound identical.

#### A2 · Error quack – Priority 2

| | |
|---|---|
| **When** | Bug hunt, step 2: Harold quacks at the bug on his screen, 1.2 s after A3 |
| **Intent** | The same voice as A1, shorter and sharper: "Hey. You." |
| **Length** | 100–300 ms |
| **Level** | −6 dB |
| **Note** | Can be a variation of A1 |

#### A3 · Error spotted – Priority 2

| | |
|---|---|
| **When** | Bug hunt, step 1: Harold notices a bug |
| **Intent** | "Huh?" – curious, not alarmed |
| **Picture it** | A head tilt |
| **Length** | 100–300 ms |
| **Level** | −6 dB |

#### A4 · Hop – Priority 2

| | |
|---|---|
| **When** | Now and then, a happy little hop on the spot |
| **Intent** | Light, quick, pleased with himself |
| **Picture it** | A small duck bouncing once and landing on both feet on a wooden desk |
| **Length** | 50–250 ms |
| **Level** | −9 dB |
| **Avoid** | A cartoon "boing", a rabbit hop, a falling "game over" sweep |

#### A5 · Footsteps, left and right – Priority 1

| | |
|---|---|
| **When** | Every 300 ms, for as long as Harold walks the cursor (minutes at a time) |
| **Intent** | Small webbed feet on a desk: light, soft, a little wet |
| **Length** | 30–100 ms each |
| **Level** | −15 dB |
| **Files** | 3 variations of each foot (6 files); the app alternates them |
| **Avoid** | Anything that sounds like a person stomping, shoes, or heavy impacts |

Footsteps must sit in the background and never tire the ear. This matters more than any other detail: they play more often than everything else together.

### Family B: the device

#### B1 · Wake – Priority 1

| | |
|---|---|
| **When** | OK wakes Harold up |
| **Intent** | "I'm on it" – the family motif, going up |
| **Length** | 150–300 ms |
| **Level** | −3 dB |

#### B2 · Sleep – Priority 1

| | |
|---|---|
| **When** | OK puts Harold to sleep |
| **Intent** | "Going to sleep" – B1's mirror: the motif going down, softer |
| **Length** | 200–400 ms |
| **Level** | −3 dB |

#### B3 · Stopped – Priority 2

| | |
|---|---|
| **When** | The Stop after timer ran out and Harold went to sleep on his own. The Flipper also buzzes. |
| **Intent** | "Done for the day" – a slower, longer, settled version of B2 |
| **Length** | 500–900 ms |
| **Level** | −3 dB |

#### B4 · Link lost – Priority 1

| | |
|---|---|
| **When** | The computer went away for more than 3 seconds while Harold was awake. The Flipper also buzzes. |
| **Intent** | "Something's wrong" – heard from across the room, but not an alarm |
| **Length** | 300–600 ms |
| **Level** | 0 dB, the loudest sound |

#### B5 · Link back – Priority 1

| | |
|---|---|
| **When** | The computer came back |
| **Intent** | "All good again" – B4's mirror |
| **Length** | 150–400 ms |
| **Level** | −3 dB |

#### B6 · Error fixed – Priority 2

| | |
|---|---|
| **When** | Bug hunt, step 3: 0.7 s after the error quack, the bug is fixed |
| **Intent** | "Got it" – small, satisfied, a little smug; the family's smallest "success" |
| **Length** | 150–400 ms |
| **Level** | −6 dB |

The bug hunt is one little scene: A3 → (1.2 s) → A2 → (0.7 s) → B6. Make the three work as a sequence.

#### B7 · Tick – Priority 3

| | |
|---|---|
| **When** | Subtle mode moves the cursor by one pixel, every minute or two |
| **Intent** | Barely there |
| **Length** | under 20 ms |
| **Level** | −18 dB |
| **Note** | The current click works; replace it only if it doesn't fit the family |

## The speaker

The Flipper Zero has a small coin-type speaker: **87 dB, 100–2500 Hz**.

- Nothing above 2.5 kHz comes out. Brightness and air are lost; design so the sound still reads without them.
- Below about 300 Hz it gets weak. The body of a sound should sit between 300 Hz and 2.5 kHz.
- Duckwalk will play recordings at **16 kHz with 8-bit loudness steps** (in development; today it plays tones). Quiet tails and long fades turn grainy, so prefer short sounds with clean endings over long decays.
- Mono only. No reverb or room: the speaker can't carry it and it turns to mush.

**Check your mix:** listen through a 300 Hz–2.5 kHz band-pass at low volume. If it still reads, it will work on the Flipper. Final approval is always on a real Flipper.

## Delivery

| | |
|---|---|
| **Format** | WAV, mono, 48 kHz (or 44.1 kHz), 24-bit (or 16-bit) |
| **Trim** | No silence before the sound; at most 20 ms after |
| **Loudness** | Loudest sound (B4) peaks at −1 dBFS; the others at their level from the cards |
| **Effects** | No reverb, no stereo, no limiter pumping |
| **Names** | `dw_<card>_<name>_v<n>.wav`, for example `dw_a1_quack_v1.wav`, `dw_a5_step_left_v2.wav` |
| **Optional** | Your own 16 kHz, 8-bit version, so we hear what you heard |

We convert the masters for the Flipper. The 16 kHz, 8-bit files will live on the Flipper's SD card, so size is not a concern.

### Delivery checklist

- [ ] A1 Quack (2–3 variations)
- [ ] A2 Error quack
- [ ] A3 Error spotted
- [ ] A4 Hop
- [ ] A5 Footsteps, 3 left and 3 right
- [ ] B1 Wake
- [ ] B2 Sleep
- [ ] B3 Stopped
- [ ] B4 Link lost
- [ ] B5 Link back
- [ ] B6 Error fixed
- [ ] B7 Tick (optional)
- [ ] Every file trimmed, mono, named, at its level

## Rounds

| Round | What you send | What we send back |
|---|---|---|
| **1 · Sketches** | Priority 1 only: 2 rough options each, plus the Family B instrument and motif | Which option, in one line per sound |
| **2 · Full set** | Every card, one version each | Notes per card, heard on a Flipper |
| **3 · Masters** | Final files, named and levelled | Sign-off, then they go into the app |

Feedback always names the card ("A5: too heavy, lighter feet"), so nothing is ambiguous.

## Notes from earlier attempts

What didn't work, so you don't have to rediscover it:

- **Chiptune and "8-bit" sounds** felt fake and out of place, even though the Flipper looks retro.
- **Everything as a quack** blurred the meaning: you could no longer tell a device sound from Harold.
- **Footsteps** made from real recordings sounded like a person stomping. Harold is small and light.
- **The quack** made from tones sounds like a bicycle horn.
- **The hop** as a rising sweep sounded like an arcade "game over"; a cartoon spring sounded like a rabbit.

## Licence

Duckwalk is open source under GPL-3.0, and its sounds ship with the app. Every sound must be your own original work, released under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) or [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); with CC BY you are credited by name. Nothing from sample libraries with other licences, and nothing taken from games or films.

[← Brand](brand.md) · [Docs home](README.md) · [Next: Decisions →](decisions/0001-tell-duckwalk-from-the-user.md)
