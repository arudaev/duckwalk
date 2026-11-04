/* The simulator has no speaker: every sound is silent. */

#include "duckwalk.h"

void sounds_init(void) {
}

void sounds_free(void) {
}

void sounds_play(Sound sound, uint8_t level, bool vibro) {
    UNUSED(sound);
    UNUSED(level);
    UNUSED(vibro);
}

void duckwalk_sound(Duckwalk* app, Sound sound) {
    UNUSED(app);
    UNUSED(sound);
}
