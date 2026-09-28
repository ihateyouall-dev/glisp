#pragma once

#include "location.h"
#include "value.h"

#define GL_PRAGMA(Name)                                                                            \
    gl_value_t *gl_pragma_##Name(gl_location_t called_at, gl_function_args_t *args)

GL_PRAGMA(gc_allocated);
GL_PRAGMA(gc_collect);
GL_PRAGMA(gc_toggle);
GL_PRAGMA(gc_enabled_p);

gl_value_t *gl_dispatch_pragma(gl_location_t called_at, const char *name, gl_function_args_t *args);
