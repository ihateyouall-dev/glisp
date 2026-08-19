#pragma once

#include "value.h"

typedef struct {
    gl_value_t *head;
    size_t allocated;
} gl_memory_heap_t;

extern gl_memory_heap_t *gl_memory_heap;

void gl_init_memory_heap(void);

void gl_register_value(gl_value_t *val);

void gl_gc_collect(void);
