#pragma once

#define CONS_DECLARE(T, Name)                                                                      \
    typedef struct {                                                                               \
        T car;                                                                                     \
        T cdr;                                                                                     \
    } Name##_t;
