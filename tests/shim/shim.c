/* Host implementations of the firmware calls declared in tests/shim. */

#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

/* Records: the tested code only opens and closes them. */

void* furi_record_open(const char* name) {
    UNUSED(name);
    return NULL;
}

void furi_record_close(const char* name) {
    UNUSED(name);
}

/* Ticks */

static uint32_t tick_now = 1000;

uint32_t furi_get_tick(void) {
    return tick_now;
}

uint32_t furi_kernel_get_tick_frequency(void) {
    return 1000;
}

void shim_tick_set(uint32_t tick) {
    tick_now = tick;
}

/* FuriString */

struct FuriString {
    char* text;
};

FuriString* furi_string_alloc(void) {
    FuriString* string = calloc(1, sizeof(FuriString));
    string->text = calloc(1, 1);
    return string;
}

void furi_string_free(FuriString* string) {
    free(string->text);
    free(string);
}

void furi_string_set_str(FuriString* string, const char* text) {
    free(string->text);
    string->text = malloc(strlen(text) + 1);
    strcpy(string->text, text);
}

const char* furi_string_get_cstr(const FuriString* string) {
    return string->text;
}

bool furi_string_equal_str(const FuriString* string, const char* text) {
    return strcmp(string->text, text) == 0;
}

/* Random: xorshift32, seeded by the tests. */

static uint32_t random_state = 0x2545F491u;

void shim_random_seed(uint32_t seed) {
    random_state = seed ? seed : 1;
}

uint32_t furi_hal_random_get(void) {
    uint32_t x = random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return random_state = x;
}

/* Clock */

static DateTime clock_now = {.hour = 12, .weekday = 3};

void shim_rtc_set(uint8_t weekday, uint8_t hour) {
    clock_now.weekday = weekday;
    clock_now.hour = hour;
}

void furi_hal_rtc_get_datetime(DateTime* datetime) {
    *datetime = clock_now;
}

/* Storage */

char shim_app_data_dir[256] = ".";

const char* shim_app_data_path(const char* name) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/%s", shim_app_data_dir, name);
    return path;
}

/* Flipper Format */

#define MAX_LINES 64

struct FlipperFormat {
    char* lines[MAX_LINES];
    size_t count;
    size_t cursor; /* reads search forward from here, like the firmware */
    FILE* out;
};

FlipperFormat* flipper_format_file_alloc(Storage* storage) {
    UNUSED(storage);
    return calloc(1, sizeof(FlipperFormat));
}

void flipper_format_free(FlipperFormat* file) {
    for(size_t i = 0; i < file->count; i++) free(file->lines[i]);
    if(file->out) fclose(file->out);
    free(file);
}

bool flipper_format_file_open_existing(FlipperFormat* file, const char* path) {
    FILE* in = fopen(path, "r");
    if(!in) return false;
    char buf[256];
    while(file->count < MAX_LINES && fgets(buf, sizeof(buf), in)) {
        buf[strcspn(buf, "\r\n")] = 0;
        file->lines[file->count] = malloc(strlen(buf) + 1);
        strcpy(file->lines[file->count++], buf);
    }
    fclose(in);
    return true;
}

bool flipper_format_file_open_always(FlipperFormat* file, const char* path) {
    file->out = fopen(path, "w");
    return file->out != NULL;
}

bool flipper_format_rewind(FlipperFormat* file) {
    file->cursor = 0;
    return true;
}

static bool read_key(FlipperFormat* file, const char* key, FuriString* value) {
    size_t key_len = strlen(key);
    for(size_t i = file->cursor; i < file->count; i++) {
        const char* line = file->lines[i];
        if(strncmp(line, key, key_len) == 0 && line[key_len] == ':' && line[key_len + 1] == ' ') {
            furi_string_set_str(value, line + key_len + 2);
            file->cursor = i + 1;
            return true;
        }
    }
    return false;
}

bool flipper_format_read_header(FlipperFormat* file, FuriString* filetype, uint32_t* version) {
    FuriString* text = furi_string_alloc();
    bool ok = read_key(file, "Filetype", filetype) && read_key(file, "Version", text);
    if(ok) {
        char* end;
        unsigned long v = strtoul(furi_string_get_cstr(text), &end, 10);
        ok = *end == 0 && end != furi_string_get_cstr(text);
        *version = (uint32_t)v;
    }
    furi_string_free(text);
    return ok;
}

bool flipper_format_read_string(FlipperFormat* file, const char* key, FuriString* value) {
    return read_key(file, key, value);
}

bool flipper_format_write_header_cstr(FlipperFormat* file, const char* filetype, uint32_t version) {
    if(!file->out) return false;
    return fprintf(file->out, "Filetype: %s\nVersion: %lu\n", filetype, (unsigned long)version) > 0;
}

bool flipper_format_write_string_cstr(FlipperFormat* file, const char* key, const char* value) {
    if(!file->out) return false;
    return fprintf(file->out, "%s: %s\n", key, value) > 0;
}
