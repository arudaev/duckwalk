#pragma once

#include <stdint.h>

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday; /* 1 = Monday ... 7 = Sunday, as on the Flipper */
} DateTime;

void furi_hal_rtc_get_datetime(DateTime* datetime);

/* Tests set the clock the app reads. */
void shim_rtc_set(uint8_t weekday, uint8_t hour);
