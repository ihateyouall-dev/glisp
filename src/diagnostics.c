#include "diagnostics.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void gl_diagnostic_show_frame(const char *src, gl_location_t loc) {
    char *line = __get_line(src, loc.line);
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
    gl_diagnostic_show_frame(src, loc);
    fprintf(stderr, "Syntax error: %s\n", msg);
}
