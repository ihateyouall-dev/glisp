#pragma once

#include "ast.h"
#include "lexer.h"

typedef struct gl_parser_t {
    const char *src;
    gl_lex_token_vector_t tokens;
    size_t pos;
    const char *name;
} gl_parser_t;

void gl_parser_init(gl_parser_t *parser, const char *src, const char *name);
gl_parser_t gl_make_parser(const char *src, const char *name);
void gl_parser_destroy(gl_parser_t *parser);
gl_ast_node_t *gl_parser_parse(gl_parser_t *parser);
