#pragma once

#include "utils/cons.h"
#include <stddef.h>
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

    union {
        int64_t integral;
        long double floating;
        char *symbol;

        gl_ast_cons_t cons;
    } value;
} gl_ast_node_t;

gl_ast_node_t *gl_ast_cons_nth_car(gl_ast_node_t *cons, size_t n);
gl_ast_node_t *gl_ast_cons_nth_cdr(gl_ast_node_t *cons, size_t n);
size_t gl_ast_cons_length(gl_ast_node_t *cons);

gl_ast_node_t *gl_ast_make_quote(gl_ast_node_t *node);
gl_ast_node_t *gl_ast_make_funquote(gl_ast_node_t *node);

gl_ast_node_t *gl_ast_make_int(int64_t num);
gl_ast_node_t *gl_ast_make_float(long double num);
gl_ast_node_t *gl_ast_make_symbol(const char *sym);
gl_ast_node_t *gl_ast_make_cons(gl_ast_node_t *car, gl_ast_node_t *cdr);
gl_ast_node_t *gl_ast_make_nil(void);
gl_ast_node_t *gl_ast_copy(gl_ast_node_t *node);
void gl_ast_destroy(gl_ast_node_t *node);
