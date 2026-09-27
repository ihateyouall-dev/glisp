#include "ansi.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

int test_status = 0;

int main(void) {
    char *res1 = gl_process_ansi_str("Hello, World\\n");
    TEST(strcmp(res1, "Hello, World\n") == 0, "Newline");
    free(res1);

    char *res2 = gl_process_ansi_str("Col1\\tCol2\\r\\n");
    TEST(strcmp(res2, "Col1\tCol2\r\n") == 0, "Tab and CR");
    free(res2);

    char *res3 = gl_process_ansi_str("\\e[31mRed\\e[0m");
    TEST(strcmp(res3, "\x1b[31mRed\x1b[0m") == 0, "ANSI escape \\e");
    free(res3);

    char *res4 = gl_process_ansi_str("\\033[32mGreen\\033[0m");
    TEST(strcmp(res4, "\x1b[32mGreen\x1b[0m") == 0, "ANSI escape octal \\033");
    free(res4);

    char *res5 = gl_process_ansi_str("\\x1b[1;34mBlue\\x1b[0m");
    TEST(strcmp(res5, "\x1b[1;34mBlue\x1b[0m") == 0, "ANSI escape hex \\x1b");
    free(res5);

    char buf[] = "Inplace\\nTest";
    gl_process_ansi_str_inplace(buf);
    TEST(strcmp(buf, "Inplace\nTest") == 0, "In-place processing");

    return test_status;
}
