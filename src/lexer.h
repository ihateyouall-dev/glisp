#pragma once

#include "vector.h"

#include <stddef.h>

typedef enum {
    LEX_EOF,
    LEX_LPAREN,
    LEX_RPAREN,
    LEX_INTLITERAL,
    LEX_FLOATLITERAL,
    LEX_SYMBOL,
    LEX_UNKNOWN
} LexTokenType;

typedef enum { LEX_OK, LEX_ERROR } LexTokenStatus;

typedef struct {
    size_t pos;
    size_t line;
    size_t column;
} Location;

typedef struct {
    LexTokenType type;
    LexTokenStatus status;
    char *value;

    Location location;
} LexToken;

typedef struct {
    const char *src;
    size_t len;

    Location location;
} Lexer;

VECTOR_DECLARE(LexToken, LexTokenVector)

void lexer_init(Lexer *lexer, const char *src);
Lexer make_lexer(const char *src);
char lexer_current(Lexer *lexer);
char lexer_peek(Lexer *lexer);
void lexer_advance(Lexer *lexer);
LexToken lexer_next(Lexer *lexer);
LexTokenVector lexer_tokenize(Lexer *lexer);
