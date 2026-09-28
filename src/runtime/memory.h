#pragma once

#include "value.h"

typedef struct {
    gl_value_t *head;
    size_t allocated;
} gl_memory_heap_t;

extern gl_memory_heap_t *gl_memory_heap;
extern int gl_gc_enabled;

void gl_init_memory_heap(void);

void gl_register_value(gl_value_t *val);

void gl_register_source(char *buf, char *unit_name);

void gl_destroy_sources(void);

void gl_gc_collect(void);

void gl_destroy_memory_heap(void);
