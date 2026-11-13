#pragma once

/**
 * @file diagnostics.h
 * @brief Error and diagnostic reporting API.
 */

#include "utils/location.h"

/** Categories of reported errors. */
typedef enum {
    GL_SYNTAX_ERROR,
    GL_TYPE_ERROR,
    GL_SYMBOL_ERROR,
    GL_ARITY_ERROR,
    GL_ASSERTION_FAIL,
    GL_DIVISION_BY_ZERO,
    GL_RECURSION_ERROR
} gl_error_type_t;

/** Severity attached to a diagnostic. */
typedef enum { GL_WARNING, GL_ERROR, GL_FATAL } gl_error_severity_t;

/** A pending diagnostic. The message is borrowed and must outlive reporting. */
typedef struct {
    gl_error_type_t type;         /**< Error category. */
    gl_error_severity_t severity; /**< Warning, error, or fatal. */
    gl_location_t location;       /**< Source position. */
    char *msg;                    /**< Human-readable message. */
} gl_error_t;

/** Allocates a diagnostic. @p msg is borrowed; the caller must keep it alive. */
gl_error_t *gl_make_error(gl_error_type_t type, gl_error_severity_t severity,
                          gl_location_t location, char *msg);

/** Prints the source line for @p loc with a caret under the column. */
void gl_diagnostic_show_frame(gl_location_t loc);

/** Prints and frees a diagnostic. */
void gl_diagnostic_report_error(gl_error_t *err);

/** Reports that exactly @p expected arguments were required but @p got were given. */
void gl_diagnostic_report_arity_error(gl_location_t location, size_t expected, size_t got);

/** Reports that at least @p expected_at_least arguments were required. */
void gl_diagnostic_report_variadic_arity_error(gl_location_t location, size_t expected_at_least,
                                               size_t got);

/** Reports that the argument count must be in [min_expected, max_expected]. */
void gl_diagnostic_report_range_arity_error(gl_location_t location, size_t min_expected,
                                            size_t max_expected, size_t got);
