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

// Single source of truth for the CLI version and the REPL banner.
#define GLISP_VERSION "0"

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

// Evaluates one -e expression in the global environment. Returns 0 on success.
static int __gl_eval_expression(const char *expr) {
    gl_parser_t parser = gl_make_parser(expr, "<eval>");
    gl_value_t *res = gl_parse_and_eval(&parser);

    gl_parser_destroy(&parser);

    if (!res) {
        return 1;
    }

    if (res->type != GL_VAL_NIL) {
        gl_value_print(res);
        printf("\n");
    }

    return 0;
}

int main(int argc, char **argv) {
    int load_stdlib = 1;
    int eval_count = 0;
    const char *file_to_run = NULL;

    // Parse CLI flags; the first non-option argument is the script to run.
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--no-stdlib") == 0) {
            load_stdlib = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options] [file]\n\n", argv[0]);
            printf("Options:\n");
            printf("  --no-stdlib      Do not autoload standard library\n");
            printf("  -e, --eval EXPR  Evaluate an expression and exit; may be repeated\n");
            printf("  -h, --help       Show this help message\n");
            printf("  -v, --version    Show version and exit\n");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("glisp %s\n", GLISP_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--eval") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Option %s requires an expression\n", argv[i]);
                return 1;
            }
            ++eval_count;
            ++i;
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

    if (eval_count > 0 && file_to_run) {
        fprintf(stderr, "Cannot combine -e with a script file '%s'\n", file_to_run);
        return 1;
    }

    // Build the runtime and, unless disabled, evaluate the standard library.
    gl_init_global_env();

    if (load_stdlib) {
        gl_load_stdlib();
    }

    // Eval mode: evaluate each -e expression in order and exit.
    // Evaluating expressions if given
    if (eval_count > 0) {
        int status = 0;

        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-e") != 0 && strcmp(argv[i], "--eval") != 0) {
                continue;
            }
            if (__gl_eval_expression(argv[++i]) != 0) {
                status = 1;
            }
        }

        gl_call_stack_cleanup();
        gl_destroy_memory_heap();
        return status;
    }

    // Batch mode: evaluate the given file and exit.
    // Evaluating a file if given
    if (file_to_run) {
        gl_value_t *res = gl_load_file(file_to_run, NULL);
        if (!res) {
            fprintf(stderr, "Cannot load file '%s'\n", file_to_run);
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
    printf("GLisp REPL version %s\n", GLISP_VERSION);

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

        gl_value_t *res = gl_parse_and_eval(&parser);

        gl_parser_destroy(&parser);

        gl_value_print(res);

        free(buffer);
    }
    gl_call_stack_cleanup();
    gl_destroy_memory_heap();
    return 0;
}
