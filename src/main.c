#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    // Evaluating a file if given
    if (argc == 2) {
        // Evaluation
        const char *filename = argv[1];

        FILE *file = fopen(filename, "r");

        if (!file) {
            perror(filename);
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
        buf[bytesread] = '\0';

        printf("File:\n%s", buf);
        fclose(file);
        free(buf);
        return 0;
    }

    // Launching REPL otherwise
    puts("GLisp REPL version 0");
    while (1) {
        char *input = readline("\nglisp> ");

        add_history(input);

        // Must be replaced with evaluation
        printf("You are passed: %s\n", input);

        free(input);
    }
    return 0;
}
