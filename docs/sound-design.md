# 10 · Sound design

[← Brand](brand.md) · [Docs home](README.md) · [Next: Decisions →](decisions/0001-tell-duckwalk-from-the-user.md)

A brief for whoever designs Duckwalk's sounds: what plays when, what each sound has to say, and what the Flipper's speaker can take.

## Harold

Harold is a rubber debugging duck who lives on a Flipper Zero. He sits at a tiny desk reading code, walks the computer's cursor around so the computer stays awake, hunts bugs in the code and quacks at them. He is cheeky, a little smug and always busy, never menacing.

The feel we are after is a small cartoon animal getting on with its own business on your desk. Untitled Goose Game and Desktop Goose are the reference for that feel: short, dry, comic sounds that are funny because of timing, not volume. Use them for mood only. Don't sample, trace or imitate any of their sounds.

## The sounds

Settings → Sound has three levels. **Cues** plays only the sounds that tell you something changed; **Lively** adds Harold's own sounds. Off is silent. Harold's own sounds play only while he is on screen.

"Keep" means we are happy with the current sound; a better idea is still welcome. "Wanted" means we need a new one.

### Cues

| Sound | When it plays | What it should say | Length | Status |
|---|---|---|---|---|
| Wake | OK wakes Harold up | "I'm on it" – up, bright | 150–250 ms | Keep: three notes up |
| Sleep | OK puts him to sleep | "Going to sleep" – down, soft | 200–300 ms | Keep: three notes down |
| Stopped | Stop after ran out and Harold went to sleep on his own | "Done for the day" – a yawn, settling | 500–900 ms | Keep: a yawn, then two notes down |
| Link lost | The computer went away for more than 3 seconds while Harold is awake | "Something's wrong" – must be heard from across the room | 300–600 ms | Keep: two notes down |
| Link back | The computer came back | "All good again" – the opposite of Link lost | 150–300 ms | Keep: two notes up |
| Quack | You hold OK | Harold quacks back at you | 150–400 ms | **Wanted** |

Stopped and Link lost also buzz the Flipper's vibration motor; the sound should work with or without it.

### Harold's own sounds (Lively)

| Sound | When it plays | What it should say | Length | Status |
|---|---|---|---|---|
| Error spotted | Harold notices a bug in the code on his laptop | "Huh?" – curious, not alarmed | 100–300 ms | **Wanted** (we have a candidate) |
| Error quack | 1.2 s later he quacks at the bug | The Quack, a little higher, quicker and quieter | 100–300 ms | **Wanted**, made from the Quack |
| Error fixed | 0.7 s after that the bug is fixed | "Got it" – small, satisfied, a little smug | 150–400 ms | **Wanted** |
| Hop | A happy little hop on the spot | A light, quick bounce | 50–200 ms | **Wanted** |
| Footstep, left and right | Every 300 ms while he walks the cursor | Small webbed feet on a desk | 20–80 ms | **Wanted** (the current tone pats are the best so far) |
| Nudge | Subtle mode moves the cursor by one pixel | Barely there – a tick | under 20 ms | Keep: a click |

Footsteps play for minutes at a time, so they must sit in the background and never tire the ear. Two or three variations of each foot are welcome; the app can pick one at random.

### The quack

The quack is the most important sound: it is Harold's voice and the one people will remember. It should sound like a rubber duck – a squeezed toy with a cheeky, nasal "quack" – not a bicycle horn and not a real duck. The current tone version sounds too much like a horn; a recorded cartoon duck was closer, but it isn't a rubber duck either. The Error quack is the same voice and can be the same recording, pitched up and shortened.

## The speaker

The Flipper Zero has a small piezo speaker. It is loud and clear in the upper mid-range and has almost no bass: anything below about 400 Hz is weak, and the low end of a recording disappears. Bright, clean sounds with a clear pitch carry best. Always listen on a real Flipper before calling a sound done; a laptop's speakers are kinder than the piezo.

A sound can reach the speaker in one of two ways.

1. **A tone sequence.** The speaker plays one square-ish wave at a time; each step sets a pitch (in Hz), a length (1 ms or more) and a volume (0 to 1). Changing the pitch every few milliseconds makes slides, buzzes and rasps. This is how every sound is made today. A step's volume also changes its colour: quieter steps are thinner, louder steps fuller.
2. **A recording.** Mono, 8 kHz, 8-bit. This playback is still in development, so a sound that works as a tone sequence is the safer bet. At 8 kHz nothing above 4 kHz survives, and at 8 bits quiet tails turn grainy, so recordings should be short, dry and loud.

## What to deliver

- For a recording: a WAV file, mono, 44.1 or 48 kHz, 16-bit, trimmed tight with no silence at either end and no reverb. We convert it to the Flipper's format. If you can, include your own 8 kHz, 8-bit version too, so we hear what you heard.
- For a tone sequence: a list of steps (pitch in Hz, length in ms, volume 0–1), or a recording of the idea plus a short description, and we write the steps.
- Keep every sound under 1 second, and the whole set small: the app carries every recording inside it, and an 8 kHz recording costs 8 KB a second.
- Name files after the sounds above (`quack.wav`, `error-fixed.wav`, `step-left-1.wav`).

## Licence

Duckwalk is open source under GPL-3.0, and its sounds ship inside the app. Every sound has to be your own original work, released under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) or [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); with CC BY you are credited by name in the docs. Nothing from sample libraries with other licences, and nothing taken from games or films.

[← Brand](brand.md) · [Docs home](README.md) · [Next: Decisions →](decisions/0001-tell-duckwalk-from-the-user.md)
