/* Just enough PNG for the simulator, on top of zlib: read the 1-bit and
 * 8-bit sprites in app/images, write RGB screenshots. */

#include "png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static uint8_t* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = n > 0 ? malloc((size_t)n) : NULL;
    if(data && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if(pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

uint8_t* png_read_gray(const char* path, int* width, int* height) {
    size_t size;
    uint8_t* file = read_file(path, &size);
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if(!file || size < 33 || memcmp(file, sig, 8) != 0) {
        free(file);
        return NULL;
    }

    uint32_t w = 0, h = 0;
    uint8_t depth = 0, type = 0, interlace = 0;
    uint8_t palette[256 * 3] = {0};
    uint8_t* idat = NULL;
    size_t idat_len = 0;
    for(size_t pos = 8; pos + 12 <= size;) {
        uint32_t len = be32(file + pos);
        const uint8_t* tag = file + pos + 4;
        const uint8_t* data = file + pos + 8;
        if(pos + 12 + len > size) break;
        if(!memcmp(tag, "IHDR", 4)) {
            w = be32(data);
            h = be32(data + 4);
            depth = data[8];
            type = data[9];
            interlace = data[12];
        } else if(!memcmp(tag, "PLTE", 4)) {
            memcpy(palette, data, len < sizeof(palette) ? len : sizeof(palette));
        } else if(!memcmp(tag, "IDAT", 4)) {
            idat = realloc(idat, idat_len + len);
            memcpy(idat + idat_len, data, len);
            idat_len += len;
        }
        pos += 12 + len;
    }
    free(file);

    /* Gray, RGB, palette or RGBA; no interlacing, no 16-bit. */
    int channels = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 6 ? 4 : 0;
    if(!w || !h || !channels || interlace || depth > 8 || !idat) {
        free(idat);
        return NULL;
    }
    size_t bpp = (channels * depth + 7) / 8;
    size_t stride = (w * channels * depth + 7) / 8;
    uLongf raw_len = (uLongf)((stride + 1) * h);
    uint8_t* raw = malloc(raw_len);
    if(uncompress(raw, &raw_len, idat, (uLong)idat_len) != Z_OK) {
        free(idat);
        free(raw);
        return NULL;
    }
    free(idat);

    /* Undo the per-row filters in place. */
    for(uint32_t y = 0; y < h; y++) {
        uint8_t* row = raw + y * (stride + 1);
        uint8_t* cur = row + 1;
        uint8_t* prev = y ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        for(size_t i = 0; i < stride; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev ? prev[i] : 0, c = prev && i >= bpp ? prev[i - bpp] : 0;
            switch(row[0]) {
            case 1:
                cur[i] += a;
                break;
            case 2:
                cur[i] += b;
                break;
            case 3:
                cur[i] += (a + b) / 2;
                break;
            case 4:
                cur[i] += paeth(a, b, c);
                break;
            }
        }
    }

    uint8_t* gray = malloc((size_t)w * h);
    for(uint32_t y = 0; y < h; y++) {
        const uint8_t* cur = raw + y * (stride + 1) + 1;
        for(uint32_t x = 0; x < w; x++) {
            int value;
            if(depth < 8) {
                int bit = x * depth, max = (1 << depth) - 1;
                value = (cur[bit / 8] >> (8 - depth - bit % 8)) & max;
                if(type == 0) value = value * 255 / max;
            } else {
                value = cur[x * channels];
            }
            if(type == 3) value = palette[value * 3];
            if(type == 2 || type == 6) value = (cur[x * channels] + cur[x * channels + 1] + cur[x * channels + 2]) / 3;
            if(type == 6 && cur[x * channels + 3] < 128) value = 255; /* transparent is paper */
            gray[y * w + x] = (uint8_t)value;
        }
    }
    free(raw);
    *width = (int)w;
    *height = (int)h;
    return gray;
}

static bool write_chunk(FILE* f, const char* tag, const uint8_t* data, uint32_t len) {
    uint8_t head[8];
    put_be32(head, len);
    memcpy(head + 4, tag, 4);
    uLong crc = crc32(crc32(0, NULL, 0), head + 4, 4);
    crc = crc32(crc, data, len);
    uint8_t tail[4];
    put_be32(tail, (uint32_t)crc);
    return fwrite(head, 1, 8, f) == 8 && fwrite(data, 1, len, f) == len && fwrite(tail, 1, 4, f) == 4;
}

bool png_write_rgb(const char* path, int width, int height, const uint8_t* rgb) {
    size_t stride = (size_t)width * 3;
    uLong raw_len = (uLong)((stride + 1) * height);
    uint8_t* raw = malloc(raw_len);
    for(int y = 0; y < height; y++) {
        raw[y * (stride + 1)] = 0; /* no filter */
        memcpy(raw + y * (stride + 1) + 1, rgb + y * stride, stride);
    }
    uLongf packed_len = compressBound(raw_len);
    uint8_t* packed = malloc(packed_len);
    bool ok = compress2(packed, &packed_len, raw, raw_len, 9) == Z_OK;
    free(raw);

    uint8_t ihdr[13] = {0};
    put_be32(ihdr, (uint32_t)width);
    put_be32(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8; /* bit depth */
    ihdr[9] = 2; /* RGB */
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    FILE* f = ok ? fopen(path, "wb") : NULL;
    ok = f && fwrite(sig, 1, 8, f) == 8 && write_chunk(f, "IHDR", ihdr, 13) &&
         write_chunk(f, "IDAT", packed, (uint32_t)packed_len) && write_chunk(f, "IEND", NULL, 0);
    if(f) fclose(f);
    free(packed);
    return ok;
}
