#pragma once

#include <stdint.h>

typedef enum { GL_AST_INT, GL_AST_FLOAT, GL_AST_SYMBOL, GL_AST_CONS, GL_AST_NIL } gl_ast_node_type;

typedef struct gl_ast_node {
    gl_ast_node_type type;

    union {
        int64_t integral;
        long double floating;
        char *symbol;

        struct {
            struct gl_ast_node *car;
            struct gl_ast_node *cdr;
        } cons;
    } value;
} gl_ast_node;

gl_ast_node *gl_ast_make_int(int64_t num);
gl_ast_node *gl_ast_make_float(long double num);
gl_ast_node *gl_ast_make_symbol(const char *sym);
gl_ast_node *gl_ast_make_cons(gl_ast_node *car, gl_ast_node *cdr);
gl_ast_node *gl_ast_make_nil(void);
void gl_ast_destroy(gl_ast_node *node);
