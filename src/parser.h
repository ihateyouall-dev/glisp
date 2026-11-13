#pragma once

/**
 * @file parser.h
 * @brief Parser state and the top-level parse API.
 */

#include "ast.h"
#include "lexer.h"

/** Parser state over a pre-tokenized source unit. */
typedef struct gl_parser_t {
    const char *src;              /**< Borrowed source buffer. */
    gl_lex_token_vector_t tokens; /**< Owned token stream. */
    size_t pos;                   /**< Index of the next token. */
} gl_parser_t;

/** Initializes a parser and tokenizes @p src. */
void gl_parser_init(gl_parser_t *parser, const char *src, const char *name);

/** Creates a parser by value. */
gl_parser_t gl_make_parser(const char *src, const char *name);

/** Frees the token stream; the source buffer is not freed. */
void gl_parser_destroy(gl_parser_t *parser);

/**
 * Parses the next top-level expression.
 *
 * @return a newly allocated AST tree, or NULL at end of input or on error.
 *         The caller owns the returned tree (see gl_ast_destroy).
 */
gl_ast_node_t *gl_parser_parse(gl_parser_t *parser);
