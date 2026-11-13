#pragma once

/**
 * @file memory.h
 * @brief Heap management, collection, and temporary-root API.
 */

#include "value.h"

typedef struct {
    gl_value_t *head;
    size_t allocated;
} gl_memory_heap_t;

extern gl_memory_heap_t *gl_memory_heap;
extern int gl_gc_enabled;

/** Initializes the global heap; called lazily by gl_register_value. */
void gl_init_memory_heap(void);

/** Links a newly allocated value into the GC heap. */
void gl_register_value(gl_value_t *val);

void gl_register_source(char *buf, char *unit_name);

void gl_destroy_sources(void);

/** Performs a full mark-and-sweep collection. */
void gl_gc_collect(void);

/** Returns the current temporary-root stack depth. */
size_t gl_gc_root_mark(void);

/** Protects a value that is reachable only from a C local. */
void gl_gc_push_root(gl_value_t *val);

/** Pops temporary roots back to a depth previously returned by gl_gc_root_mark. */
void gl_gc_roots_release(size_t mark);

/**
 * Collects when the heap exceeds GL_GC_THRESHOLD.
 *
 * Must be called only at a safe point where all live values are reachable from
 * the global environment, a call frame, or the temporary-root stack.
 */
void gl_gc_maybe_collect(void);

/** Frees every value, registered source, and the root stack. */
void gl_destroy_memory_heap(void);
