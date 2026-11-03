#pragma once

#include <furi.h>
#include <storage/storage.h>

/* A small Flipper Format: "Key: value" lines in a text file, the first two
 * being Filetype and Version. Enough of the firmware's API for settings.c,
 * with the same return values for missing files and keys. */

typedef struct FlipperFormat FlipperFormat;

FlipperFormat* flipper_format_file_alloc(Storage* storage);
void flipper_format_free(FlipperFormat* file);

bool flipper_format_file_open_existing(FlipperFormat* file, const char* path);
bool flipper_format_file_open_always(FlipperFormat* file, const char* path);
bool flipper_format_rewind(FlipperFormat* file);

bool flipper_format_read_header(FlipperFormat* file, FuriString* filetype, uint32_t* version);
bool flipper_format_read_string(FlipperFormat* file, const char* key, FuriString* value);

bool flipper_format_write_header_cstr(FlipperFormat* file, const char* filetype, uint32_t version);
bool flipper_format_write_string_cstr(FlipperFormat* file, const char* key, const char* value);
