/**
 * @file main.c
 * @brief Command-line entry point: environment setup, file execution, and the REPL.
 */

#include "parser.h"
#include "runtime/builtins.h"
#include "runtime/callstack.h"
#include "runtime/env.h"
#include "runtime/eval.h"
#include "runtime/memory.h"
#include "runtime/value.h"
#include "utils/rl_wrapper.h"

#include <assert.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include "utils/strdup.h"
#endif

// Counts unclosed brackets, skipping string literals and line comments, so the
// REPL knows whether a form is complete. A negative result means the line closed
// more than it opened; the parser then reports the real error.
static int __gl_bracket_balance(const char *line) {
    int balance = 0;
    int in_string = 0;

    for (const char *p = line; *p != '\0'; ++p) {
        if (in_string) {
            if (*p == '\\' && p[1] != '\0') {
                ++p;
            } else if (*p == '"') {
                in_string = 0;
            }
            continue;
        }

        if (*p == ';') {
            break;
        }
        if (*p == '"') {
            in_string = 1;
        } else if (*p == '(' || *p == '[' || *p == '{') {
            ++balance;
        } else if (*p == ')' || *p == ']' || *p == '}') {
            --balance;
        }
    }

    return balance;
}

int main(int argc, char **argv) {
    int load_stdlib = 1;
    const char *file_to_run = NULL;

    // Parse CLI flags; the first non-option argument is the script to run.
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--no-stdlib") == 0) {
            load_stdlib = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options] [file]\n\n", argv[0]);
            printf("Options:\n");
            printf("  --no-stdlib    Do not autoload standard library\n");
            printf("  -h, --help     Show this help message\n");
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            if (!file_to_run) {
                file_to_run = argv[i];
            } else {
                fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
                return 1;
            }
        }
    }

    // Build the runtime and, unless disabled, evaluate the standard library.
    gl_init_global_env();

    if (load_stdlib) {
        gl_load_stdlib();
    }

    // Batch mode: evaluate the given file and exit.
    // Evaluating a file if given
    if (file_to_run) {
        gl_value_t *res = gl_load_file(file_to_run, NULL);
        if (!res) {
            perror(file_to_run);
            gl_call_stack_cleanup();
            gl_destroy_memory_heap();
            return -1;
        }

        gl_call_stack_cleanup();
        gl_destroy_memory_heap();
        return 0;
    }

    // Interactive mode: accumulate lines until the brackets balance, then
    // evaluate the whole form. Launching REPL otherwise
    puts("GLisp REPL version 0");

    while (1) {
        char *buffer = NULL;
        size_t buffer_len = 0;
        size_t buffer_cap = 0;
        int balance = 0;
        int discard = 0;

        int need_more = 1;

        while (need_more) {
            char *input = readline(buffer == NULL ? "\nglisp> " : "... ");

            if (input == NULL) {
                // Ctrl-D drops a pending form and exits from the main prompt.
                if (buffer != NULL) {
                    discard = 1;
                }
                break;
            }

            if (input[0] != '\0') {
                add_history(input);
            } else if (buffer != NULL) {
                // An empty line abandons the unfinished form.
                discard = 1;
                free(input);
                break;
            }

            size_t line_len = strlen(input);
            size_t needed = buffer_len + line_len + 2;
            if (needed > buffer_cap) {
                buffer_cap = needed;
                buffer = realloc(buffer, buffer_cap);
            }
            if (buffer_len > 0) {
                buffer[buffer_len++] = '\n';
            }
            memcpy(buffer + buffer_len, input, line_len + 1);
            buffer_len += line_len;

            int empty_line = input[0] == '\0';
            balance += __gl_bracket_balance(input);
            free(input);

            if (empty_line) {
                break;
            }
            need_more = balance > 0;
        }

        if (discard || buffer == NULL) {
            free(buffer);
            if (discard == 0) {
                break;
            }
            continue;
        }

        gl_parser_t parser = gl_make_parser(buffer, "<repl>");

        gl_value_t *res = gl_parse_and_eval(&parser, 0);

        gl_parser_destroy(&parser);

        gl_value_print(res);

        free(buffer);
    }
    gl_call_stack_cleanup();
    gl_destroy_memory_heap();
    return 0;
}
