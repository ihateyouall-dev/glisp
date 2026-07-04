#pragma once

#include <stdio.h>

extern int test_status;

#define RED "\033[31m"
#define GREEN "\033[32m"
#define RESET "\033[0m"

#define TEST(cond, msg)                                                                            \
    do {                                                                                           \
        printf("[TEST] %s... ", msg);                                                              \
        if (cond) {                                                                                \
            puts(GREEN "OK" RESET);                                                                \
        } else {                                                                                   \
            printf(RED "FAILED" RESET " (%s:%d)\n", __FILE__, __LINE__);                           \
            test_status = 1;                                                                       \
        }                                                                                          \
    } while (0)
