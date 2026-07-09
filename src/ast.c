#include "ast.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// MSVC has _strdup instead of strdup
#ifdef _MSC_VER
#define strdup _strdup
#endif

gl_ast_node *gl_ast_make_int(int64_t num) {
    gl_ast_node *res = malloc(sizeof(gl_ast_node));
    assert(res);
    res->type = GL_AST_INT;
    res->value.integral = num;
    return res;
}

gl_ast_node *gl_ast_make_float(long double num) {
    gl_ast_node *res = malloc(sizeof(gl_ast_node));
    assert(res);
    res->type = GL_AST_FLOAT;
    res->value.floating = num;
    return res;
}

gl_ast_node *gl_ast_make_symbol(const char *sym) {
    gl_ast_node *res = malloc(sizeof(gl_ast_node));
    assert(res);
    res->type = GL_AST_SYMBOL;
    res->value.symbol = strdup(sym);
    return res;
}

gl_ast_node *gl_ast_make_cons(gl_ast_node *car, gl_ast_node *cdr) {
    gl_ast_node *res = malloc(sizeof(gl_ast_node));
    assert(res);
    res->type = GL_AST_CONS;
    res->value.cons.car = car;
    res->value.cons.cdr = cdr;
    return res;
}

gl_ast_node *gl_ast_make_nil(void) {
    gl_ast_node *res = malloc(sizeof(gl_ast_node));
    assert(res);
    res->type = GL_AST_NIL;
    return res;
}

void gl_ast_destroy(gl_ast_node *node) {
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
        break;
    }

    free(node);
}
