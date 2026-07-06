#include "lexer.h"
#include "test.h"

#include <string.h>

int test_status = 0;

void lexer_test(void) {
    const char *src = "(1 3.14 abc ; comment\n\" 42.a$%+-><)";

    Lexer lexer = make_lexer(src);

    TEST(lexer.len == strlen(src), "Length");

    LexToken token = lexer_next(&lexer);

    TEST(token.type == LEX_LPAREN, "LPAREN");
    TEST(token.location.pos == 0, "LPAREN pos");
    TEST(token.location.line == 1, "LPAREN line");
    TEST(token.location.column == 1, "LPAREN column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_INTLITERAL, "INTLITERAL");
    TEST(strcmp(token.value, "1") == 0, "INTLITERAL value");
    TEST(token.location.pos == 1, "INTLITERAL pos");
    TEST(token.location.line == 1, "INTLITERAL line");
    TEST(token.location.column == 2, "INTLITERAL column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_FLOATLITERAL, "FLOATLITERAL");
    TEST(strcmp(token.value, "3.14") == 0, "FLOATLITERAL value");
    TEST(token.location.pos == 3, "FLOATLITERAL pos");
    TEST(token.location.line == 1, "FLOATLITERAL line");
    TEST(token.location.column == 4, "FLOATLITERAL column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_SYMBOL, "SYMBOL");
    TEST(strcmp(token.value, "abc") == 0, "SYMBOL value");
    TEST(token.location.pos == 8, "SYMBOL pos");
    TEST(token.location.line == 1, "SYMBOL line");
    TEST(token.location.column == 9, "SYMBOL column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_UNKNOWN, "UNKNOWN");
    TEST(token.status == LEX_ERROR, "UNKNOWN status");
    TEST(token.location.pos == 22, "UNKNOWN pos");
    TEST(token.location.line == 2, "UNKNOWN line");
    TEST(token.location.column == 1, "UNKNOWN column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_SYMBOL, "Complex SYMBOL");
    TEST(strcmp(token.value, "42.a$%+-><") == 0, "Complex SYMBOL value");
    TEST(token.location.pos == 24, "Complex SYMBOL pos");
    TEST(token.location.line == 2, "Complex SYMBOL line");
    TEST(token.location.column == 3, "Complex SYMBOL column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_RPAREN, "RPAREN");
    TEST(token.location.pos == 34, "RPAREN pos");
    TEST(token.location.line == 2, "RPAREN line");
    TEST(token.location.column == 13, "RPAREN column");

    token = lexer_next(&lexer);

    TEST(token.type == LEX_EOF, "EOF");
    TEST(token.location.pos == 35, "EOF pos");
    TEST(token.location.line == 2, "EOF line");
    TEST(token.location.column == 14, "EOF column");
}

void lexer_tokenize_test(void) {
    Lexer lexer = make_lexer("(1 3.14 abc ; comment\n\" 42.a$%+-><)");

    LexTokenVector tokens = lexer_tokenize(&lexer);

    TEST(tokens.size == 8, "Tokens amount");

    TEST(LexTokenVector_at(&tokens, 0)->type == LEX_LPAREN, "Token access");
    TEST(LexTokenVector_at(&tokens, 1)->type == LEX_INTLITERAL, "Token access");
    TEST(LexTokenVector_at(&tokens, 2)->type == LEX_FLOATLITERAL, "Token access");
    TEST(LexTokenVector_at(&tokens, 3)->type == LEX_SYMBOL, "Token access");
    TEST(LexTokenVector_at(&tokens, 4)->type == LEX_UNKNOWN, "Token access");
    TEST(LexTokenVector_at(&tokens, 5)->type == LEX_SYMBOL, "Token access");
    TEST(LexTokenVector_at(&tokens, 6)->type == LEX_RPAREN, "Token access");
    TEST(LexTokenVector_at(&tokens, 7)->type == LEX_EOF, "Token access");
}

int main(void) {
    lexer_test();
    lexer_tokenize_test();

    return test_status;
}
