#pragma once

#include "utils/location.h"

void gl_diagnostic_show_frame(const char *src, gl_location_t loc);

void gl_diagnostic_syntax_error(const char *src, gl_location_t loc, const char *msg,
                                const char *name);
