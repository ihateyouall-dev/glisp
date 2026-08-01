#pragma once

#include "utils/cons.h"
#include <stdint.h>

typedef enum {
    GL_AST_INT,
    GL_AST_FLOAT,
    GL_AST_SYMBOL,
    GL_AST_CONS,
    GL_AST_NIL
} gl_ast_node_type_t;

struct gl_ast_node_t;

CONS_DECLARE(struct gl_ast_node_t *, gl_ast_cons)

typedef struct gl_ast_node_t {
    gl_ast_node_type_t type;
    int quoted; // Flag indicating if this node was parsed/constructed as a literal quote block

    union {
        int64_t integral;
        long double floating;
        char *symbol;

        gl_ast_cons_t cons;
    } value;
} gl_ast_node_t;

gl_ast_node_t *gl_ast_make_int(int64_t num, int quoted);
gl_ast_node_t *gl_ast_make_float(long double num, int quoted);
gl_ast_node_t *gl_ast_make_symbol(const char *sym, int quoted);
gl_ast_node_t *gl_ast_make_cons(gl_ast_node_t *car, gl_ast_node_t *cdr, int quoted);
gl_ast_node_t *gl_ast_make_nil(void);
void gl_ast_destroy(gl_ast_node_t *node);
