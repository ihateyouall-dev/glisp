#pragma once

/**
 * @file alloc.h
 * @brief Allocation helpers that stop the process when the heap is exhausted.
 *
 * A failed allocation is not recoverable here: containers and runtime values
 * are updated in place, so continuing after a partial allocation would leave
 * the object graph inconsistent and turn a clean out-of-memory condition into
 * memory corruption. These helpers report the failure and abort instead.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void gl_alloc_fail(void) {
    fputs("glisp: out of memory\n", stderr);
    abort();
}

/** Like malloc, but never returns NULL. */
static inline void *gl_xmalloc(size_t size) {
    // A zero-sized request is legal for malloc but not portable in its result;
    // one byte keeps the returned pointer unique and freeable.
    void *res = malloc(size == 0 ? 1 : size);
    if (res == NULL) {
        gl_alloc_fail();
    }
    return res;
}

/** Like calloc, but never returns NULL. */
static inline void *gl_xcalloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        count = 1;
        size = 1;
    }
    void *res = calloc(count, size);
    if (res == NULL) {
        gl_alloc_fail();
    }
    return res;
}

/** Like realloc, but never returns NULL. */
static inline void *gl_xrealloc(void *ptr, size_t size) {
    void *res = realloc(ptr, size == 0 ? 1 : size);
    if (res == NULL) {
        gl_alloc_fail();
    }
    return res;
}

/** Copies a NUL-terminated string into a new allocation. */
static inline char *gl_xstrdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *res = gl_xmalloc(len);
    memcpy(res, str, len);
    return res;
}
