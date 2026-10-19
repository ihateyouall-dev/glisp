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

#ifdef MSC_VER
#include "utils/strdup.h"
#endif

int main(int argc, char **argv) {
    int load_stdlib = 1;
    const char *file_to_run = NULL;

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

    gl_init_global_env();

    if (load_stdlib) {
        gl_load_stdlib();
    }

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

    // Launching REPL otherwise
    puts("GLisp REPL version 0");

    while (1) {
        char *input = readline("\nglisp> ");
        if (!input) {
            break;
        }

        if (input[0] != '\0') {
            add_history(input);
        }

        gl_parser_t parser = gl_make_parser(input, "<repl>");

        gl_value_t *res = gl_parse_and_eval(&parser, 0);

        gl_parser_destroy(&parser);

        gl_value_print(res);

        free(input);
    }
    gl_call_stack_cleanup();
    gl_destroy_memory_heap();
    return 0;
}
