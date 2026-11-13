#pragma once

/**
 * @file lexer.h
 * @brief Token types, lexer state, and the tokenization API.
 */

#include "utils/location.h"
#include "utils/vector.h"

#include <stddef.h>

/** Token kinds produced by the lexer. */
typedef enum {
    GL_LEX_EOF,
    GL_LEX_LPAREN,
    GL_LEX_RPAREN,
    GL_LEX_QUOTE,
    GL_LEX_FUNQUOTE,
    GL_LEX_INTLITERAL,
    GL_LEX_FLOATLITERAL,
    GL_LEX_SYMBOL,
    GL_LEX_UNKNOWN,
    GL_LEX_STRLITERAL,
    // Square parentheses
    GL_LEX_LSPAREN,
    GL_LEX_RSPAREN,
    // Figure parentheses
    GL_LEX_LFPAREN,
    GL_LEX_RFPAREN
} gl_lex_token_type_t;

/** Whether a token was read successfully. */
typedef enum { LEX_OK, LEX_ERROR } gl_lex_token_status_t;

/** A single token with its source location. */
typedef struct gl_lex_token_t {
    gl_lex_token_type_t type;     /**< Token kind. */
    gl_lex_token_status_t status; /**< LEX_ERROR for malformed input. */
    char *value;                  /**< Token text for literals and symbols. */

    gl_location_t location; /**< Position of the token in the source. */
} gl_lex_token_t;

void gl_lex_token_destroy(gl_lex_token_t *tok);

/** Lexer state. The source buffer is borrowed and must outlive the lexer. */
typedef struct gl_lexer_t {
    size_t len;             /**< Source length in bytes. */
    gl_location_t location; /**< Current position in the source. */
} gl_lexer_t;

VECTOR_DECLARE(gl_lex_token_t, gl_lex_token_vector)

/** Initializes @p lexer over @p src. @p unit_name is borrowed for diagnostics. */
void gl_lexer_init(gl_lexer_t *lexer, const char *src, char *unit_name);

/** Creates a lexer by value; equivalent to gl_lexer_init on a local struct. */
gl_lexer_t gl_make_lexer(const char *src, char *unit_name);

/** Returns the current character, or '\0' at end of input. */
char gl_lexer_current(gl_lexer_t *lexer);

/** Returns the next character without consuming it. */
char gl_lexer_peek(gl_lexer_t *lexer);

/** Advances one character, updating line and column counters. */
void gl_lexer_advance(gl_lexer_t *lexer);

/** Returns the next token; returns a GL_LEX_EOF token at end of input. */
gl_lex_token_t gl_lexer_next(gl_lexer_t *lexer);

/** Tokenizes the whole source unit, including the trailing EOF token. */
gl_lex_token_vector_t gl_lexer_tokenize(gl_lexer_t *lexer);
