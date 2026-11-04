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

/* Builds every sound once; call before the first sounds_play. */
void sounds_init(void);
void sounds_free(void);

/* Plays the sound if `level` lets it through, and buzzes for the warnings
 * when `vibro` is on. Never blocks for long: the notification service plays
 * it on its own thread. The Flipper's volume and stealth mode still apply. */
void sounds_play(Sound sound, uint8_t level, bool vibro);
