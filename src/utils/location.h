#pragma once

#include <stddef.h>

typedef struct gl_location_t {
    size_t pos;
    size_t line;
    size_t column;
} gl_location_t;
