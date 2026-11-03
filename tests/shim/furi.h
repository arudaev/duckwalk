#pragma once

/* Host stand-in for the parts of <furi.h> that the tested app files use.
 * Only what they need: types, furi_check and FuriString. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_check(x)                                                           \
    do {                                                                        \
        if(!(x)) {                                                              \
            fprintf(stderr, "furi_check failed: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
            abort();                                                            \
        }                                                                       \
    } while(0)

#define UNUSED(x) (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

/* Logging goes nowhere on the host. */
#define FURI_LOG_E(tag, ...) ((void)(tag))
#define FURI_LOG_W(tag, ...) ((void)(tag))
#define FURI_LOG_I(tag, ...) ((void)(tag))
#define FURI_LOG_D(tag, ...) ((void)(tag))

/* Ticks are milliseconds, as on the Flipper; tests set the clock. */
uint32_t furi_get_tick(void);
uint32_t furi_kernel_get_tick_frequency(void);
#define furi_ms_to_ticks(ms) ((uint32_t)(ms))
void shim_tick_set(uint32_t tick);

/* Opaque on the host; the tested code only stores pointers to them. */
typedef struct FuriMutex FuriMutex;

#define RECORD_STORAGE "storage"
void* furi_record_open(const char* name);
void furi_record_close(const char* name);

typedef struct FuriString FuriString;
FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* string);
void furi_string_set_str(FuriString* string, const char* text);
const char* furi_string_get_cstr(const FuriString* string);
bool furi_string_equal_str(const FuriString* string, const char* text);
