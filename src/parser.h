#pragma once

#include "ast.h"
#include "lexer.h"

typedef struct {
    gl_lex_token_vector tokens;
    size_t pos;
} gl_parser;

void gl_parser_init(gl_parser *parser, const char *src);
gl_parser gl_make_parser(const char *src);
void gl_parser_destroy(gl_parser *parser);
gl_ast_node *gl_parser_parse(gl_parser *parser);
