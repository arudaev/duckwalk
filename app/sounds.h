#pragma once

#include <furi.h>

/* Harold's sounds. Each one belongs to a level: the cues tell you something
 * (you pressed OK, Harold went to sleep, the computer went away), the lively
 * ones are Harold going about his day. Settings picks how much you hear. */

typedef enum {
    SoundLevelOff,
    SoundLevelCues,
    SoundLevelLively,
    SoundLevelCount,
} SoundLevel;

typedef enum {
    /* Cues */
    SoundQuack, /* hold OK */
    SoundWake, /* OK wakes Harold */
    SoundSleep, /* OK puts him to sleep */
    SoundStopped, /* Stop after ran out: he went to sleep on his own */
    SoundLinkLost, /* the computer went away */
    SoundLinkBack, /* and came back */
    /* Lively */
    SoundBugSpotted, /* "Bug!" */
    SoundBugQuack, /* he quacks at it */
    SoundBugFixed, /* "Fixed." */
    SoundHop, /* a happy little bounce */
    SoundStepLeft, /* footsteps during a walk */
    SoundStepRight,
    SoundNudge, /* Subtle's one pixel */
    SoundCount,
} Sound;

/* Starts the sound worker; call before the first sounds_play. */
void sounds_init(void);
void sounds_free(void);

/* Queues the sound if `level` lets it through, and a warning buzz if enabled.
 * Playback runs on the sound worker. */
void sounds_play(Sound sound, uint8_t level, bool vibro);
