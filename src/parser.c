#include "parser.h"
#include "ast.h"
#include "lexer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void gl_parser_init(gl_parser *parser, const char *src) {
    gl_lexer lexer = gl_make_lexer(src);
    parser->tokens = gl_lexer_tokenize(&lexer);
    parser->pos = 0;
}

gl_parser gl_make_parser(const char *src) {
    gl_parser res;
    gl_parser_init(&res, src);
    return res;
}

void gl_parser_destroy(gl_parser *parser) {
    gl_lex_token_vector_destroy(&parser->tokens);
    parser->pos = 0;
}

static gl_lex_token __gl_parser_current_token(gl_parser *parser) {
    return *gl_lex_token_vector_at(&parser->tokens, parser->pos);
}

static void __gl_parser_advance(gl_parser *parser) { ++parser->pos; }

static gl_ast_node *__gl_parser_parse_int(gl_parser *parser) {
    gl_lex_token token = __gl_parser_current_token(parser);

    char *endptr;
    errno = 0;
    int64_t val = strtoll(token.value, &endptr, 10);

    if (errno == ERANGE) {
        fprintf(stderr, "Parser Error at line %zu, column %zu: Numeric overflow\n",
                token.location.line, token.location.column);
        exit(1);
    }

    __gl_parser_advance(parser);
    return gl_ast_make_int(val);
}

static gl_ast_node *__gl_parser_parse_symbol(gl_parser *parser) {
    gl_lex_token token = __gl_parser_current_token(parser);
    __gl_parser_advance(parser);
    return gl_ast_make_symbol(token.value);
}

static gl_ast_node *__gl_parser_parse_float(gl_parser *parser) {
    gl_lex_token token = __gl_parser_current_token(parser);

    char *endptr;
    errno = 0;
    long double val = strtold(token.value, &endptr);

    if (errno == ERANGE) {
        fprintf(stderr, "Parser Error at line %zu, column %zu: Numeric overflow\n",
                token.location.line, token.location.column);
        exit(1);
    }

    __gl_parser_advance(parser);
    return gl_ast_make_float(val);
}

static gl_ast_node *__gl_parser_parse_list(gl_parser *parser);

// Decides what parser functions will need to call next.
static gl_ast_node *__gl_parser_parse_expression(gl_parser *parser) {
    gl_lex_token token = __gl_parser_current_token(parser);

    if (token.type == GL_LEX_LPAREN) {
        __gl_parser_advance(parser); // Skip '('
        return __gl_parser_parse_list(parser);
    }

    switch (token.type) {
    case GL_LEX_INTLITERAL:
        return __gl_parser_parse_int(parser);
    case GL_LEX_FLOATLITERAL:
        return __gl_parser_parse_float(parser);
    case GL_LEX_SYMBOL:
        return __gl_parser_parse_symbol(parser);
    default:
        fprintf(stderr, "Parser Error at line %zu, column %zu: Unexpected token type %d\n",
                token.location.line, token.location.column, token.type);
        exit(1);
    }
}

static gl_ast_node *__gl_parser_parse_list(gl_parser *parser) {
    gl_ast_node *list_head = NULL;
    gl_ast_node *current_tail = NULL;

    while (1) {
        gl_lex_token token = __gl_parser_current_token(parser);

        if (token.type == GL_LEX_RPAREN) {
            __gl_parser_advance(parser);
            break;
        }

        gl_ast_node *new_node = __gl_parser_parse_expression(parser);

        if (list_head == NULL) {
            list_head = gl_ast_make_cons(new_node, gl_ast_make_nil());
            current_tail = list_head;
        } else {
            gl_ast_node *next_cons = gl_ast_make_cons(new_node, current_tail->value.cons.cdr);
            current_tail->value.cons.cdr = next_cons;
            current_tail = next_cons;
        }
    }

    return list_head;
}

gl_ast_node *gl_parser_parse(gl_parser *parser) { return __gl_parser_parse_expression(parser); }
