#include "ast.h"

#include <assert.h>
#include <stdlib.h>

#include "utils/strdup.h"

gl_ast_node_t *gl_ast_make_int(int64_t num, int quoted) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_INT;
    res->value.integral = num;
    res->quoted = quoted;
    return res;
}

gl_ast_node_t *gl_ast_make_float(long double num, int quoted) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_FLOAT;
    res->value.floating = num;
    res->quoted = quoted;
    return res;
}

gl_ast_node_t *gl_ast_make_symbol(const char *sym, int quoted) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_SYMBOL;
    res->value.symbol = strdup(sym);
    res->quoted = quoted;
    return res;
}

gl_ast_node_t *gl_ast_make_cons(gl_ast_node_t *car, gl_ast_node_t *cdr, int quoted) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_CONS;
    res->value.cons.car = car;
    res->value.cons.cdr = cdr;
    res->quoted = quoted;
    return res;
}

gl_ast_node_t *gl_ast_make_nil(void) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_NIL;
    res->quoted = 0;
    return res;
}

void gl_ast_destroy(gl_ast_node_t *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
    case GL_AST_CONS:
        gl_ast_destroy(node->value.cons.car);
        gl_ast_destroy(node->value.cons.cdr);
        break;
    case GL_AST_SYMBOL:
        free(node->value.symbol);
        break;
    default:
        // INT, FLOAT, NIL - no dynamic memory to free per node.
        break;
    }

    free(node);
}
