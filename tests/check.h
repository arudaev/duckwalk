#pragma once

/* A minimal test harness: CHECK records a failure and carries on, so one
 * run reports every broken expectation. main() returns check_result(). */

#include <stdio.h>

static int check_failures = 0;
static int check_count = 0;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        check_count++;                                                \
        if(!(cond)) {                                                 \
            check_failures++;                                         \
            fprintf(stderr, "%s:%d: CHECK(%s) failed: ", __FILE__, __LINE__, #cond); \
            fprintf(stderr, __VA_ARGS__);                             \
            fprintf(stderr, "\n");                                    \
        }                                                             \
    } while(0)

static int check_result(const char* name) {
    printf("%s: %d checks, %d failed\n", name, check_count, check_failures);
    return check_failures ? 1 : 0;
}
