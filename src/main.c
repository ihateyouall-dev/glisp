#include "runtime/env.h"
#include "runtime/eval.h"
#include "runtime/value.h"
#include <assert.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    // Evaluating a file if given
    if (argc == 2) {
        const char *filename = argv[1];

        FILE *file = fopen(filename, "r");

        if (!file) {
            perror(filename);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        size_t filesize = ftell(file);
        rewind(file);

        char *buf = malloc(filesize + 1);

        if (!buf) {
            fclose(file);
            return -1;
        }

        size_t bytesread = fread(buf, sizeof(char), filesize, file);
        assert(bytesread == filesize);
        buf[bytesread] = '\0';

        gl_parse_and_eval(buf, gl_make_global_env());
        fclose(file);
        free(buf);
        return 0;
    }

    // Launching REPL otherwise
    puts("GLisp REPL version 0");
    while (1) {
        gl_env_t *global_env = gl_make_global_env();
        char *input = readline("\nglisp> ");

        add_history(input);

        gl_value_print(gl_parse_and_eval(input, global_env));

        free(input);
    }
    return 0;
}
