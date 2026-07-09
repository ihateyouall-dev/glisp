#pragma once

#include "vector.h"

#include <stddef.h>

typedef enum {
    GL_LEX_EOF,
    GL_LEX_LPAREN,
    GL_LEX_RPAREN,
    GL_LEX_QUOTE,
    GL_LEX_INTLITERAL,
    GL_LEX_FLOATLITERAL,
    GL_LEX_SYMBOL,
    GL_LEX_UNKNOWN
} gl_lex_token_type;

typedef enum { LEX_OK, LEX_ERROR } gl_lex_token_status;

typedef struct {
    size_t pos;
    size_t line;
    size_t column;
} gl_location;

typedef struct {
    gl_lex_token_type type;
    gl_lex_token_status status;
    char *value;

    gl_location location;
} gl_lex_token;

typedef struct {
    const char *src;
    size_t len;

    gl_location location;
} gl_lexer;

VECTOR_DECLARE(gl_lex_token, gl_lex_token_vector)

void gl_lexer_init(gl_lexer *lexer, const char *src);
gl_lexer gl_make_lexer(const char *src);
char gl_lexer_current(gl_lexer *lexer);
char gl_lexer_peek(gl_lexer *lexer);
void gl_lexer_advance(gl_lexer *lexer);
gl_lex_token gl_lexer_next(gl_lexer *lexer);
gl_lex_token_vector gl_lexer_tokenize(gl_lexer *lexer);
