/* Harold's recordings and the app's notification cues share one playback
 * queue so a footstep and a chime cannot fight over the speaker. */

#include "sounds.h"
#include "sounds_samples.h"
#include "speaker_pcm.h"

#include <notification/notification.h>
#include <notification/notification_messages.h>

#define STEP_MS 6 /* one pitch step of a sweep */
#define SOUND_QUEUE_LENGTH 6

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

typedef struct {
    const uint8_t* data;
    size_t count;
    uint8_t gain;
} Sample;

static const Sample samples[SoundCount] = {
    [SoundQuack] = {quack_sample, sizeof(quack_sample), 100},
    [SoundBugQuack] = {quack_sample, sizeof(quack_sample), 80},
    [SoundStepLeft] = {step_left_sample, sizeof(step_left_sample), 100},
    [SoundStepRight] = {step_right_sample, sizeof(step_right_sample), 100},
};

typedef struct {
    Sound sound;
    bool play_audio;
    bool buzz;
} SoundRequest;

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
    FuriMessageQueue* queue;
    FuriThread* worker;
    uint8_t sample_volume;
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
    case SoundBugFixed:
        tone(b, G6, 45, 0.5f);
        rest(b, 25);
        tone(b, C7, 90, 0.5f);
        break;
    case SoundHop:
        sweep(b, 420, 900, 60, 0.35f, 0);
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
        if(samples[s].data) continue;
        sounds.start[s] = b->step_count;
        build(b, s);
    }
}

static int32_t sound_worker(void* context) {
    UNUSED(context);
    while(true) {
        SoundRequest request;
        furi_check(furi_message_queue_get(sounds.queue, &request, FuriWaitForever) == FuriStatusOk);
        if(request.sound == SoundCount) return 0;
        if(request.buzz) notification_message_block(sounds.notifications, &sequence_buzz);
        if(!request.play_audio) continue;

        const Sample* sample = &samples[request.sound];
        if(sample->data) {
            speaker_pcm_play(sample->data, sample->count, sounds.sample_volume * sample->gain / 100);
        } else {
            const NotificationSequence* sequence =
                (const NotificationSequence*)&sounds.steps[sounds.start[request.sound]];
            notification_message_block(sounds.notifications, sequence);
        }
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
    sounds.sample_volume = speaker_pcm_volume();
    sounds.queue = furi_message_queue_alloc(SOUND_QUEUE_LENGTH, sizeof(SoundRequest));
    sounds.worker = furi_thread_alloc_ex("DuckwalkSound", 2048, sound_worker, NULL);
    furi_thread_start(sounds.worker);
}

void sounds_free(void) {
    /* Drop pending cues and let the current one release the speaker. */
    furi_message_queue_reset(sounds.queue);
    const SoundRequest stop = {.sound = SoundCount};
    furi_check(furi_message_queue_put(sounds.queue, &stop, FuriWaitForever) == FuriStatusOk);
    furi_thread_join(sounds.worker);
    furi_thread_free(sounds.worker);
    furi_message_queue_free(sounds.queue);
    sounds.queue = NULL;
    free(sounds.steps);
    free(sounds.messages);
    furi_record_close(RECORD_NOTIFICATION);
    sounds.notifications = NULL;
}

void sounds_play(Sound sound, uint8_t level, bool vibro) {
    if((unsigned)sound >= SoundCount || !sounds.queue) return;
    const SoundRequest request = {
        .sound = sound,
        .play_audio = level >= info[sound].level,
        .buzz = vibro && info[sound].buzz,
    };
    if(request.play_audio || request.buzz) furi_message_queue_put(sounds.queue, &request, 0);
}
