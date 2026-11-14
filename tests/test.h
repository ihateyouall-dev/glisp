#pragma once

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern int test_status;

#define RED "\033[31m"
#define GREEN "\033[32m"
#define RESET "\033[0m"

// Test logs are colored only when stdout is a terminal and NO_COLOR is unset.
static inline int __test_use_color(void) {
    static int cached = -1;
    const char *no_color = NULL;

    if (cached >= 0) {
        return cached;
    }

    no_color = getenv("NO_COLOR");

#ifdef _WIN32
    cached = _isatty(_fileno(stdout));
#else
    cached = isatty(fileno(stdout));
#endif

    if (no_color != NULL && no_color[0] != '\0') {
        cached = 0;
    }

    return cached;
}

#define TEST(cond, msg)                                                                            \
    do {                                                                                           \
        int use_color = __test_use_color();                                                        \
        printf("[TEST] %s... ", msg);                                                              \
        if (cond) {                                                                                \
            puts(use_color ? GREEN "OK" RESET : "OK");                                             \
        } else {                                                                                   \
            printf(use_color ? RED "FAILED" RESET " (%s:%d)\n" : "FAILED (%s:%d)\n", __FILE__,     \
                   __LINE__);                                                                      \
            test_status = 1;                                                                       \
        }                                                                                          \
    } while (0)
