/**
 * @file diagnostics.c
 * @brief Diagnostic rendering: messages, source snippets, and arity errors.
 */

#include "diagnostics.h"

#include "utils/ansi.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// Decides once whether diagnostics may emit color: stderr has to be a terminal
// and NO_COLOR has to be unset or empty.
static int __gl_use_color(void) {
    static int cached = -1;
    const char *no_color = NULL;

    if (cached >= 0) {
        return cached;
    }

    no_color = getenv("NO_COLOR");

#ifdef _WIN32
    cached = _isatty(_fileno(stderr));
#else
    cached = isatty(fileno(stderr));
#endif

    if (no_color != NULL && no_color[0] != '\0') {
        cached = 0;
    }

    return cached;
}

// Returns the escape when color is allowed, an empty string otherwise.
static const char *__gl_color(const char *escape) { return __gl_use_color() ? escape : ""; }

gl_error_t *gl_make_error(gl_error_type_t type, gl_error_severity_t severity,
                          gl_location_t location, char *msg) {
    gl_error_t *res = malloc(sizeof(gl_error_t));
    res->type = type;
    res->severity = severity;
    res->location = location;
    res->msg = msg;
    return res;
}

// Extracts a heap-allocated copy of the given 1-based line.
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

// Prints the source line followed by a caret under the offending column.
void gl_diagnostic_show_frame(gl_location_t loc) {
    char *line = __get_line(loc.src, loc.line);
    const char *gutter = __gl_color(GL_ANSI_FG_BLUE);
    const char *caret = __gl_color(GL_ANSI_BOLD GL_ANSI_FG_RED);
    const char *reset = __gl_color(GL_ANSI_RESET);

    fprintf(stderr, "%s|%s %s\n", gutter, reset, line);
    free(line);
    fprintf(stderr, "%s|%s %s", gutter, reset, caret);
    for (size_t i = 1; i < loc.column; ++i) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "^%s\n", reset);
}

void gl_diagnostic_syntax_error(const char *src, gl_location_t loc, const char *msg,
                                const char *name) {
    const char *bold = __gl_color(GL_ANSI_BOLD);
    const char *red = __gl_color(GL_ANSI_FG_RED);
    const char *reset = __gl_color(GL_ANSI_RESET);

    fprintf(stderr, "%s:%zu:%zu:\n", name, loc.line, loc.column);
    gl_diagnostic_show_frame(loc);
    fprintf(stderr, "%s%sSyntax error:%s %s\n", bold, red, reset, msg);
}

void gl_diagnostic_report_error(gl_error_t *err) {
    const char *bold = __gl_color(GL_ANSI_BOLD);
    const char *accent =
        __gl_color(err->severity == GL_WARNING ? GL_ANSI_FG_YELLOW : GL_ANSI_FG_RED);
    const char *reset = __gl_color(GL_ANSI_RESET);

    fprintf(stderr, "%s%s", bold, accent);
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
    fprintf(stderr, "%s", reset);

    fprintf(stderr, "%s:%zu:%zu:\n", err->location.unit_name, err->location.line,
            err->location.column);

    fprintf(stderr, "%s%s", bold, accent);
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
    case GL_ASSERTION_FAIL:
        // Message for assertions is optional
        if (err->msg) {
            fprintf(stderr, "Assertion failed: ");
        } else {
            fprintf(stderr, "Assertion failed.");
        }
        break;
    case GL_DIVISION_BY_ZERO:
        fprintf(stderr, "Division by zero: ");
        break;
    case GL_OVERFLOW_ERROR:
        fprintf(stderr, "Overflow error: ");
        break;
    case GL_RECURSION_ERROR:
        fprintf(stderr, "Recursion error: ");
        break;
    }
    fprintf(stderr, "%s", reset);

    fprintf(stderr, "%s\n", (err->msg ? err->msg : ""));
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

    snprintf(buf, sizeof(buf), fmt, expected_at_least, got);

    gl_error_t *err = gl_make_error(GL_ARITY_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}

void gl_diagnostic_report_range_arity_error(gl_location_t location, size_t min_expected,
                                            size_t max_expected, size_t got) {
    const char *fmt = "Argument count mismatch. Expected from %zu to %zu, got %zu";
    char buf[128] = "";

    snprintf(buf, sizeof(buf), fmt, min_expected, max_expected, got);

    gl_error_t *err = gl_make_error(GL_ARITY_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}
