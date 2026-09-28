#include "pragma.h"

#include "diagnostics.h"
#include "memory.h"
#include "value.h"

#include <stdio.h>
#include <string.h>

#define EXPECT_ARGS(n)                                                                             \
    do {                                                                                           \
        if (args->size != n) {                                                                     \
            gl_diagnostic_report_arity_error(called_at, n, args->size);                            \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_VARGS(n)                                                                            \
    do {                                                                                           \
        if (args->size < n) {                                                                      \
            gl_diagnostic_report_variadic_arity_error(called_at, n, args->size);                   \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

GL_PRAGMA(gc_allocated) {
    EXPECT_ARGS(0);
    return gl_value_make_int(gl_memory_heap ? (int64_t)gl_memory_heap->allocated : 0);
}

GL_PRAGMA(gc_collect) {
    EXPECT_ARGS(0);
    gl_gc_collect();
    return gl_value_make_nil();
}

GL_PRAGMA(gc_toggle) {
    if (args->size == 0) {
        gl_gc_enabled = !gl_gc_enabled;
    } else if (args->size == 1) {
        gl_gc_enabled = gl_value_get_bool(args->data[0]->val);
    } else {
        gl_diagnostic_report_variadic_arity_error(called_at, 0, args->size);
        return NULL;
    }
    return gl_value_make_bool(gl_gc_enabled);
}

GL_PRAGMA(gc_enabled_p) {
    EXPECT_ARGS(0);
    return gl_value_make_bool(gl_gc_enabled);
}

gl_value_t *gl_dispatch_pragma(gl_location_t called_at, const char *name,
                               gl_function_args_t *args) {
    if (strcmp(name, "gc-allocated") == 0) {
        return gl_pragma_gc_allocated(called_at, args);
    } else if (strcmp(name, "gc-collect") == 0) {
        return gl_pragma_gc_collect(called_at, args);
    } else if (strcmp(name, "gc-toggle") == 0) {
        return gl_pragma_gc_toggle(called_at, args);
    } else if (strcmp(name, "gc-enabled?") == 0 || strcmp(name, "gc-enabled") == 0) {
        return gl_pragma_gc_enabled_p(called_at, args);
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "Unknown pragma directive '%s'", name);
    gl_error_t *err = gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, called_at, buf);
    gl_diagnostic_report_error(err);
    return NULL;
}
