#pragma once

#include <stddef.h>
#include <stdint.h>

#define SPEAKER_PCM_RATE 8000U

/* Reads the device's saved speaker level, expressed as a percentage. */
uint8_t speaker_pcm_volume(void);

/* Plays unsigned 8-bit mono PCM on the calling thread. */
void speaker_pcm_play(const uint8_t* samples, size_t count, uint8_t volume);
