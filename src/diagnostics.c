#include "diagnostics.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gl_error_t *gl_make_error(gl_error_type_t type, gl_error_severity_t severity,
                          gl_location_t location, char *msg) {
    gl_error_t *res = malloc(sizeof(gl_error_t));
    res->type = type;
    res->severity = severity;
    res->location = location;
    res->msg = msg;
    return res;
}

static char *__get_line(const char *src, int line) {
    int current_line = 1;
    const char *start = src;

    while (*start != '\0' && current_line < line) {
        if (*start == '\n') {
            current_line++;
        }
        start++;
    }

    if (*start == '\0' && current_line < line) {
        return NULL;
    }

    const char *end = start;
    while (*end != '\0' && *end != '\n') {
        end++;
    }

    size_t length = end - start;

    char *res = malloc(length + 1);

    strncpy(res, start, length);
    res[length] = '\0';

    return res;
}

void gl_diagnostic_show_frame(gl_location_t loc) {
    char *line = __get_line(loc.src, loc.line);
    fprintf(stderr, "| %s\n", line);
    free(line);
    fprintf(stderr, "| ");
    for (size_t i = 1; i < loc.column; ++i) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "^\n");
}

void gl_diagnostic_syntax_error(const char *src, gl_location_t loc, const char *msg,
                                const char *name) {
    fprintf(stderr, "%s:%zu:%zu:\n", name, loc.line, loc.column);
    gl_diagnostic_show_frame(loc);
    fprintf(stderr, "Syntax error: %s\n", msg);
}

void gl_diagnostic_report_error(gl_error_t *err) {
    switch (err->severity) {
    case GL_WARNING:
        fprintf(stderr, "WARNING: ");
        break;
    case GL_ERROR:
        fprintf(stderr, "ERROR: ");
        break;
    case GL_FATAL:
        fprintf(stderr, "FATAL: ");
        break;
    }

    fprintf(stderr, "%s:%zu:%zu:\n", err->location.unit_name, err->location.line,
            err->location.column);

    switch (err->type) {
    case GL_SYNTAX_ERROR:
        fprintf(stderr, "Syntax error: ");
        break;
    case GL_TYPE_ERROR:
        fprintf(stderr, "Type error: ");
        break;
    case GL_SYMBOL_ERROR:
        fprintf(stderr, "Symbol error: ");
        break;
    case GL_ARITY_ERROR:
        fprintf(stderr, "Arity error: ");
        break;
    }

    fprintf(stderr, "%s\n", err->msg);
    gl_diagnostic_show_frame(err->location);
    free(err);
}

void gl_diagnostic_report_arity_error(gl_location_t location, size_t expected, size_t got) {
    const char *fmt = "Argument count mismatch. Expected %zu, got %zu";
    char buf[128] = "";

    snprintf(buf, sizeof(buf), fmt, expected, got);

    gl_error_t *err = gl_make_error(GL_ARITY_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}

void gl_diagnostic_report_variadic_arity_error(gl_location_t location, size_t expected_at_least,
                                               size_t got) {
    const char *fmt = "Argument count mismatch. Expected at least %zu, got %zu";
    char buf[128] = "";

    snprintf(buf, strlen(fmt) + 1, fmt, expected_at_least, got);

    gl_error_t *err = gl_make_error(GL_ARITY_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}
