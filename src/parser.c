#include "parser.h"
#include "ast.h"
#include "diagnostics.h"
#include "lexer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gl_parser_init(gl_parser_t *parser, const char *src, const char *name) {
    gl_lexer_t lexer = gl_make_lexer(src, name); // NOLINT
    parser->tokens = gl_lexer_tokenize(&lexer);
    parser->src = src;
    parser->pos = 0;
}

gl_parser_t gl_make_parser(const char *src, const char *name) {
    gl_parser_t res;
    gl_parser_init(&res, src, name);
    return res;
}

void gl_parser_destroy(gl_parser_t *parser) {
    gl_lex_token_vector_destroy(&parser->tokens);
    parser->pos = 0;
}

static gl_lex_token_t __gl_parser_current_token(gl_parser_t *parser) {
    return *gl_lex_token_vector_at(&parser->tokens, parser->pos);
}

static void __gl_parser_advance(gl_parser_t *parser) { ++parser->pos; }

static gl_ast_node_t *__gl_parser_parse_int(gl_parser_t *parser) {
    gl_lex_token_t token = __gl_parser_current_token(parser);

    char *endptr;
    errno = 0;
    int64_t val = strtoll(token.value, &endptr, 10);

    if (errno == ERANGE) {
        fprintf(stderr, "Parser Error at line %zu, column %zu: Numeric overflow\n",
                token.location.line, token.location.column);
        exit(1);
    }

    __gl_parser_advance(parser);
    return gl_ast_make_int(val, token.location);
}

static gl_ast_node_t *__gl_parser_parse_symbol(gl_parser_t *parser) {
    gl_lex_token_t token = __gl_parser_current_token(parser);
    __gl_parser_advance(parser);
    return gl_ast_make_symbol(token.value, token.location);
}

static gl_ast_node_t *__gl_parser_parse_float(gl_parser_t *parser) {
    gl_lex_token_t token = __gl_parser_current_token(parser);

    char *endptr;
    errno = 0;
    long double val = strtold(token.value, &endptr);

    if (errno == ERANGE) {
        fprintf(stderr, "Parser Error at line %zu, column %zu: Numeric overflow\n",
                token.location.line, token.location.column);
        exit(1);
    }

    __gl_parser_advance(parser);
    return gl_ast_make_float(val, token.location);
}

static gl_ast_node_t *__gl_parser_parse_list(gl_parser_t *parser);

// Decides what parser functions will need to call next.
static gl_ast_node_t *__gl_parser_parse_expression(gl_parser_t *parser) {
    gl_lex_token_t token = __gl_parser_current_token(parser);
    int quoted = 0;
    int funquoted = 0;

    if (token.type == GL_LEX_QUOTE) {
        __gl_parser_advance(parser);
        token = __gl_parser_current_token(parser);
        quoted = 1;
        return gl_ast_make_quote(__gl_parser_parse_expression(parser));
    }

    if (token.type == GL_LEX_FUNQUOTE) {
        __gl_parser_advance(parser);
        token = __gl_parser_current_token(parser);
        funquoted = 1;
        return gl_ast_make_funquote(__gl_parser_parse_expression(parser));
    }

    if (token.type == GL_LEX_LPAREN) {
        return __gl_parser_parse_list(parser);
    }

    gl_ast_node_t *res = NULL;

    switch (token.type) {
    case GL_LEX_INTLITERAL:
        res = __gl_parser_parse_int(parser);
        break;
    case GL_LEX_FLOATLITERAL:
        res = __gl_parser_parse_float(parser);
        break;
    case GL_LEX_SYMBOL:
        res = __gl_parser_parse_symbol(parser);
        break;
    case GL_LEX_EOF:
        return NULL;
    default: {
        gl_diagnostic_report_error(gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, token.location,

                                                 "Unexpected token"));
    }
    }
    return res;
}

static gl_ast_node_t *__gl_parser_parse_list(gl_parser_t *parser) {
    gl_ast_node_t *list_head = NULL;
    gl_ast_node_t *current_tail = NULL;

    gl_lex_token_t lparen = __gl_parser_current_token(parser);
    __gl_parser_advance(parser);

    while (1) {
        gl_lex_token_t token = __gl_parser_current_token(parser);

        if (token.type == GL_LEX_RPAREN) {
            __gl_parser_advance(parser);
            // Empty list is similar to nil
            if (list_head == NULL) {
                return gl_ast_make_nil(token.location);
            }
            break;
        }

        if (token.type == GL_LEX_EOF) { // List is not closed
            gl_diagnostic_report_error(gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, lparen.location,
                                                     "Expected ')' to close expression"));
            return NULL;
        }

        gl_ast_node_t *new_node = __gl_parser_parse_expression(parser);

        // Terminating if we got error in node parsing
        if (new_node == NULL) {
            return NULL;
        }

        if (list_head == NULL) {
            list_head =
                gl_ast_make_cons(new_node, gl_ast_make_nil(token.location), lparen.location);
            current_tail = list_head;
        } else {
            gl_ast_node_t *next_cons =
                gl_ast_make_cons(new_node, current_tail->value.cons.cdr, token.location);
            current_tail->value.cons.cdr = next_cons;
            current_tail = next_cons;
        }
    }

    return list_head;
}

gl_ast_node_t *gl_parser_parse(gl_parser_t *parser) { return __gl_parser_parse_expression(parser); }
