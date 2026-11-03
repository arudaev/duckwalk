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

#define RECORD_STORAGE "storage"
void* furi_record_open(const char* name);
void furi_record_close(const char* name);

typedef struct FuriString FuriString;
FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* string);
void furi_string_set_str(FuriString* string, const char* text);
const char* furi_string_get_cstr(const FuriString* string);
bool furi_string_equal_str(const FuriString* string, const char* text);
