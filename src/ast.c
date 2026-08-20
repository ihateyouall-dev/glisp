#include "ast.h"
#include "utils/location.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "utils/strdup.h"
#endif

gl_ast_node_t *gl_ast_cons_nth_car(gl_ast_node_t *cons, size_t n) {
    if (cons->type != GL_AST_CONS) {
        return NULL;
    }
    for (size_t i = 0; i < n; ++i) {
        if (cons->value.cons.cdr->type == GL_AST_NIL) {
            return NULL;
        }
        cons = cons->value.cons.cdr;
    }
    return cons->value.cons.car;
}

gl_ast_node_t *gl_ast_cons_nth_cdr(gl_ast_node_t *cons, size_t n) {
    if (cons->type != GL_AST_CONS) {
        return NULL;
    }
    for (size_t i = 0; i < n; ++i) {
        if (cons->type == GL_AST_NIL) {
            return NULL;
        }
        cons = cons->value.cons.cdr;
    }
    return cons->value.cons.cdr;
}

size_t gl_ast_cons_length(gl_ast_node_t *cons) {
    if (cons->type != GL_AST_CONS) {
        return 0;
    }
    size_t res = 0;

    while (cons->type != GL_AST_NIL) {
        ++res;
        cons = cons->value.cons.cdr;
    }
    return res;
}

gl_ast_node_t *gl_ast_make_quote(gl_ast_node_t *node) {
    return gl_ast_make_cons(gl_ast_make_symbol("quote", node->location),
                            gl_ast_make_cons(node, gl_ast_make_nil(node->location), node->location),
                            node->location);
}

gl_ast_node_t *gl_ast_make_funquote(gl_ast_node_t *node) {
    return gl_ast_make_cons(gl_ast_make_symbol("function", node->location),
                            gl_ast_make_cons(node, gl_ast_make_nil(node->location), node->location),
                            node->location);
}

gl_ast_node_t *gl_ast_make_int(int64_t num, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_INT;
    res->value.integral = num;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_float(long double num, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_FLOAT;
    res->value.floating = num;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_symbol(const char *sym, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_SYMBOL;
    res->value.symbol = strdup(sym);
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_cons(gl_ast_node_t *car, gl_ast_node_t *cdr, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_CONS;
    res->value.cons.car = car;
    res->value.cons.cdr = cdr;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_string(char *str, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_STRING;
    res->value.string = strdup(str);
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_nil(gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_NIL;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_copy(gl_ast_node_t *node) {
    if (!node)
        return NULL;

    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);

    res->type = node->type;
    res->location = node->location;

    switch (node->type) {
    case GL_AST_INT:
    case GL_AST_FLOAT:
        res->value = node->value;
        break;
    case GL_AST_SYMBOL:
        res->value.symbol = strdup(node->value.symbol);
        break;
    case GL_AST_STRING:
        res->value.string = strdup(node->value.string);
        break;
    case GL_AST_CONS:
        res->value.cons.car = gl_ast_copy(node->value.cons.car);
        res->value.cons.cdr = gl_ast_copy(node->value.cons.cdr);
        break;
    case GL_AST_NIL:
        break;
    }
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
