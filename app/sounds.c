/* Harold's sounds, played on the Flipper's speaker.
 *
 * The speaker plays one square-wave tone at a time, so every sound is a list
 * of short tones. A duck's quack is a nasal, falling sweep: here the pitch
 * falls while it jumps up and down a little every few milliseconds, which
 * gives the rasp, and a bright click at the start is the "qu". Melodies use
 * plain notes.
 *
 * The tones are built into notification sequences once, at start, and stay
 * put until the app ends: the notification service plays a sequence later,
 * on its own thread, and reads it while it plays. */

#include "sounds.h"

#include <notification/notification.h>
#include <notification/notification_messages.h>

#define STEP_MS 6 /* one pitch step of a sweep */

typedef struct {
    NotificationMessage* messages; /* NULL on the counting pass */
    const NotificationMessage** steps;
    size_t message_count;
    size_t step_count;
} Builder;

typedef struct {
    uint8_t level; /* the lowest Sound setting that plays it */
    bool buzz; /* a warning you might miss: buzz when Vibration is on */
} SoundInfo;

static const SoundInfo info[SoundCount] = {
    [SoundQuack] = {SoundLevelCues, false},
    [SoundWake] = {SoundLevelCues, false},
    [SoundSleep] = {SoundLevelCues, false},
    [SoundStopped] = {SoundLevelCues, true},
    [SoundLinkLost] = {SoundLevelCues, true},
    [SoundLinkBack] = {SoundLevelCues, false},
    [SoundBugSpotted] = {SoundLevelLively, false},
    [SoundBugQuack] = {SoundLevelLively, false},
    [SoundBugFixed] = {SoundLevelLively, false},
    [SoundHop] = {SoundLevelLively, false},
    [SoundStepLeft] = {SoundLevelLively, false},
    [SoundStepRight] = {SoundLevelLively, false},
    [SoundNudge] = {SoundLevelLively, false},
};

static const NotificationSequence sequence_buzz = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

static struct {
    NotificationApp* notifications;
    NotificationMessage* messages;
    const NotificationMessage** steps;
    size_t start[SoundCount]; /* each sound's first step */
} sounds;

static void put(Builder* b, NotificationMessage message) {
    if(b->messages) {
        b->messages[b->message_count] = message;
        b->steps[b->step_count] = &b->messages[b->message_count];
    }
    b->message_count++;
    b->step_count++;
}

static void delay(Builder* b, uint32_t ms) {
    put(b, (NotificationMessage){.type = NotificationMessageTypeDelay, .data.delay.length = ms});
}

static void tone(Builder* b, float hz, uint32_t ms, float volume) {
    put(b,
        (NotificationMessage){
            .type = NotificationMessageTypeSoundOn,
            .data.sound = {.frequency = hz, .volume = volume},
        });
    delay(b, ms);
}

static void rest(Builder* b, uint32_t ms) {
    put(b, (NotificationMessage){.type = NotificationMessageTypeSoundOff});
    delay(b, ms);
}

/* From one pitch to another, fading to half the volume. `rasp` jumps the
 * pitch up and down by that many hertz on every step. */
static void sweep(Builder* b, float from, float to, uint32_t ms, float volume, float rasp) {
    uint32_t steps = ms / STEP_MS;
    if(steps < 2) steps = 2;
    for(uint32_t i = 0; i < steps; i++) {
        float t = (float)i / (steps - 1);
        float hz = from + (to - from) * t + (i % 2 ? rasp : -rasp);
        tone(b, hz, STEP_MS, volume * (1.0f - 0.5f * t));
    }
}

/* One quack around `pitch` hertz. */
static void quack(Builder* b, float pitch, uint32_t ms, float volume) {
    tone(b, pitch * 1.6f, 5, volume * 0.7f);
    sweep(b, pitch * 1.15f, pitch * 0.75f, ms, volume, pitch * 0.07f);
    tone(b, pitch * 0.68f, 12, volume * 0.35f);
}

/* Notes */
#define C5 523.25f
#define E5 659.25f
#define G5 783.99f
#define A5 880.00f
#define D5 587.33f
#define G4 392.00f
#define C6 1046.50f
#define E6 1318.51f
#define G6 1567.98f
#define C7 2093.00f

static void build(Builder* b, Sound sound) {
    switch(sound) {
    case SoundQuack:
        quack(b, 780, 110, 1.0f);
        rest(b, 70);
        quack(b, 720, 85, 0.9f);
        break;
    case SoundWake:
        tone(b, C6, 45, 0.7f);
        tone(b, E6, 45, 0.7f);
        tone(b, G6, 90, 0.7f);
        break;
    case SoundSleep:
        tone(b, G5, 60, 0.6f);
        tone(b, E5, 60, 0.6f);
        tone(b, C5, 140, 0.5f);
        break;
    case SoundStopped:
        /* A long yawn, then the sleep notes, slower. */
        sweep(b, 900, 430, 360, 0.6f, 10);
        rest(b, 90);
        tone(b, C5, 90, 0.5f);
        tone(b, G4, 200, 0.4f);
        break;
    case SoundLinkLost:
        tone(b, A5, 90, 0.8f);
        rest(b, 40);
        tone(b, D5, 180, 0.8f);
        break;
    case SoundLinkBack:
        tone(b, D5, 60, 0.7f);
        tone(b, A5, 100, 0.7f);
        break;
    case SoundBugSpotted:
        /* "Huh?" */
        sweep(b, 600, 1000, 90, 0.45f, 0);
        tone(b, 1000, 30, 0.3f);
        break;
    case SoundBugQuack:
        quack(b, 820, 90, 0.6f);
        break;
    case SoundBugFixed:
        tone(b, G6, 45, 0.5f);
        rest(b, 25);
        tone(b, C7, 90, 0.5f);
        break;
    case SoundHop:
        sweep(b, 420, 900, 60, 0.35f, 0);
        break;
    case SoundStepLeft:
        tone(b, 300, 10, 0.5f);
        tone(b, 160, 8, 0.3f);
        break;
    case SoundStepRight:
        tone(b, 250, 10, 0.5f);
        tone(b, 140, 8, 0.3f);
        break;
    case SoundNudge:
        tone(b, 1800, 4, 0.25f);
        break;
    default:
        break;
    }
    /* End every sound silent, then the sequence's end. */
    put(b, (NotificationMessage){.type = NotificationMessageTypeSoundOff});
    if(b->steps) b->steps[b->step_count] = NULL;
    b->step_count++;
}

static void build_all(Builder* b) {
    for(Sound s = 0; s < SoundCount; s++) {
        sounds.start[s] = b->step_count;
        build(b, s);
    }
}

void sounds_init(void) {
    sounds.notifications = furi_record_open(RECORD_NOTIFICATION);
    /* Count first, then build into storage of exactly that size. */
    Builder b = {0};
    build_all(&b);
    sounds.messages = malloc(b.message_count * sizeof(NotificationMessage));
    sounds.steps = malloc(b.step_count * sizeof(NotificationMessage*));
    b = (Builder){.messages = sounds.messages, .steps = sounds.steps};
    build_all(&b);
}

void sounds_free(void) {
    /* Let anything still playing finish before its tones are freed. */
    notification_message_block(sounds.notifications, &sequence_empty);
    free(sounds.steps);
    free(sounds.messages);
    furi_record_close(RECORD_NOTIFICATION);
    sounds.notifications = NULL;
}

void sounds_play(Sound sound, uint8_t level, bool vibro) {
    if(sound >= SoundCount || !sounds.notifications) return;
    if(info[sound].buzz && vibro) notification_message(sounds.notifications, &sequence_buzz);
    if(level >= info[sound].level) {
        notification_message(
            sounds.notifications, (const NotificationSequence*)&sounds.steps[sounds.start[sound]]);
    }
}
