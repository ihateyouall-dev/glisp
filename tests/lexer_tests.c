#include "lexer.h"
#include "test.h"

#include <string.h>

int test_status = 0;

void lexer_test(void) {
    const char *src = "(1 3.14 abc ; comment\n\" 42.a$%+-><)";

    gl_lexer_t lexer = gl_make_lexer(src);

    TEST(lexer.len == strlen(src), "Length");

    gl_lex_token_t token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_LPAREN, "LPAREN");
    TEST(token.location.pos == 0, "LPAREN pos");
    TEST(token.location.line == 1, "LPAREN line");
    TEST(token.location.column == 1, "LPAREN column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_INTLITERAL, "INTLITERAL");
    TEST(strcmp(token.value, "1") == 0, "INTLITERAL value");
    TEST(token.location.pos == 1, "INTLITERAL pos");
    TEST(token.location.line == 1, "INTLITERAL line");
    TEST(token.location.column == 2, "INTLITERAL column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_FLOATLITERAL, "FLOATLITERAL");
    TEST(strcmp(token.value, "3.14") == 0, "FLOATLITERAL value");
    TEST(token.location.pos == 3, "FLOATLITERAL pos");
    TEST(token.location.line == 1, "FLOATLITERAL line");
    TEST(token.location.column == 4, "FLOATLITERAL column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_SYMBOL, "SYMBOL");
    TEST(strcmp(token.value, "abc") == 0, "SYMBOL value");
    TEST(token.location.pos == 8, "SYMBOL pos");
    TEST(token.location.line == 1, "SYMBOL line");
    TEST(token.location.column == 9, "SYMBOL column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_UNKNOWN, "UNKNOWN");
    TEST(token.status == LEX_ERROR, "UNKNOWN status");
    TEST(token.location.pos == 22, "UNKNOWN pos");
    TEST(token.location.line == 2, "UNKNOWN line");
    TEST(token.location.column == 1, "UNKNOWN column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_SYMBOL, "Complex SYMBOL");
    TEST(strcmp(token.value, "42.a$%+-><") == 0, "Complex SYMBOL value");
    TEST(token.location.pos == 24, "Complex SYMBOL pos");
    TEST(token.location.line == 2, "Complex SYMBOL line");
    TEST(token.location.column == 3, "Complex SYMBOL column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_RPAREN, "RPAREN");
    TEST(token.location.pos == 34, "RPAREN pos");
    TEST(token.location.line == 2, "RPAREN line");
    TEST(token.location.column == 13, "RPAREN column");

    token = gl_lexer_next(&lexer);

    TEST(token.type == GL_LEX_EOF, "EOF");
    TEST(token.location.pos == 35, "EOF pos");
    TEST(token.location.line == 2, "EOF line");
    TEST(token.location.column == 14, "EOF column");
}

void lexer_tokenize_test(void) {
    gl_lexer_t lexer = gl_make_lexer("(1 3.14 abc ; comment\n\" '42.a$%+-><)");

    gl_lex_token_vector_t tokens = gl_lexer_tokenize(&lexer);

    TEST(tokens.size == 9, "Tokens amount");

    TEST(gl_lex_token_vector_at(&tokens, 0)->type == GL_LEX_LPAREN, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 1)->type == GL_LEX_INTLITERAL, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 2)->type == GL_LEX_FLOATLITERAL, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 3)->type == GL_LEX_SYMBOL, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 4)->type == GL_LEX_UNKNOWN, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 5)->type == GL_LEX_QUOTE, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 6)->type == GL_LEX_SYMBOL, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 7)->type == GL_LEX_RPAREN, "Token access");
    TEST(gl_lex_token_vector_at(&tokens, 8)->type == GL_LEX_EOF, "Token access");
}

int main(void) {
    lexer_test();
    lexer_tokenize_test();

    return test_status;
}
