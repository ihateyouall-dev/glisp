#pragma once

#include "location.h"
#include "vector.h"

#include <stddef.h>

typedef enum {
    GL_LEX_EOF,
    GL_LEX_LPAREN,
    GL_LEX_RPAREN,
    GL_LEX_QUOTE,
    GL_LEX_FUNQUOTE,
    GL_LEX_INTLITERAL,
    GL_LEX_FLOATLITERAL,
    GL_LEX_SYMBOL,
    GL_LEX_UNKNOWN
} gl_lex_token_type_t;

typedef enum { LEX_OK, LEX_ERROR } gl_lex_token_status_t;

typedef struct gl_lex_token_t {
    gl_lex_token_type_t type;
    gl_lex_token_status_t status;
    char *value;

    gl_location_t location;
} gl_lex_token_t;

void gl_lex_token_destroy(gl_lex_token_t *tok);

typedef struct gl_lexer_t {
    const char *src;
    size_t len;

    gl_location_t location;
} gl_lexer_t;

VECTOR_DECLARE(gl_lex_token_t, gl_lex_token_vector)

void gl_lexer_init(gl_lexer_t *lexer, const char *src);
gl_lexer_t gl_make_lexer(const char *src);
char gl_lexer_current(gl_lexer_t *lexer);
char gl_lexer_peek(gl_lexer_t *lexer);
void gl_lexer_advance(gl_lexer_t *lexer);
gl_lex_token_t gl_lexer_next(gl_lexer_t *lexer);
gl_lex_token_vector_t gl_lexer_tokenize(gl_lexer_t *lexer);
