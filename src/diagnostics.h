#pragma once

#include "utils/location.h"

typedef enum {
    GL_SYNTAX_ERROR,
    GL_TYPE_ERROR,
    GL_SYMBOL_ERROR,
    GL_ARITY_ERROR,
} gl_error_type_t;

typedef enum { GL_WARNING, GL_ERROR, GL_FATAL } gl_error_severity_t;

typedef struct {
    gl_error_type_t type;
    gl_error_severity_t severity;

    gl_location_t location;

    char *msg;
} gl_error_t;

gl_error_t *gl_make_error(gl_error_type_t type, gl_error_severity_t severity,
                          gl_location_t location, char *msg);

void gl_diagnostic_show_frame(gl_location_t loc);

void gl_diagnostic_report_error(gl_error_t *err);

void gl_diagnostic_report_arity_error(gl_location_t location, size_t expected, size_t got);

void gl_diagnostic_report_variadic_arity_error(gl_location_t location, size_t expected_at_least,
                                               size_t got);
