#pragma once

#include <stdint.h>

/* Deterministic on the host: tests seed it so a failure can be replayed. */
uint32_t furi_hal_random_get(void);
void shim_random_seed(uint32_t seed);
