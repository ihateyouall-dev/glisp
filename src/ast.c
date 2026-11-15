/**
 * @file ast.c
 * @brief Construction, copying, and destruction of AST nodes.
 */

#include "ast.h"

#include "utils/ansi.h"
#include "utils/location.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "utils/strdup.h"
#endif

void gl_ast_node_destroy_helper(gl_ast_node_t **node) {
    if (node && *node) {
        gl_ast_destroy(*node);
        *node = NULL;
    }
}

// Typed container instantiations used by AST nodes.
ARRAY_DEFINE(struct gl_ast_node_t *, gl_ast_array)
VECTOR_DEFINE(struct gl_ast_node_t *, gl_ast_vector)

// Returns the car of the n-th cons cell; NULL when the list is too short.
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

// Returns the cdr after skipping n cells; NULL when the list is too short.
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

// Desugars 'x into (quote x).
gl_ast_node_t *gl_ast_make_quote(gl_ast_node_t *node) {
    return gl_ast_make_cons(gl_ast_make_symbol("quote", node->location),
                            gl_ast_make_cons(node, gl_ast_make_nil(node->location), node->location),
                            node->location);
}

// Desugars #'x into (function x).
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
    res->value.string = gl_process_ansi_str(str);
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

gl_ast_node_t *gl_ast_make_array(gl_ast_array_t array, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_ARRAY;
    res->value.array = array;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_make_vector(gl_ast_vector_t vector, gl_location_t location) {
    gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
    assert(res);
    res->type = GL_AST_VECTOR;
    res->value.vector = vector;
    res->location = location;
    return res;
}

gl_ast_node_t *gl_ast_copy(gl_ast_node_t *node) {
    if (!node) {
        return NULL;
    }

    if (node->type == GL_AST_CONS) {
        // Copy the spine iteratively; the car may still recurse into nested
        // structures, but flat lists of any length are safe.
        gl_ast_node_t *res = malloc(sizeof(gl_ast_node_t));
        assert(res);

        gl_ast_node_t *dst = res;
        gl_ast_node_t *cur = node;
        while (cur != NULL && cur->type == GL_AST_CONS) {
            dst->type = GL_AST_CONS;
            dst->location = cur->location;
            dst->value.cons.car = gl_ast_copy(cur->value.cons.car);

            gl_ast_node_t *next = cur->value.cons.cdr;
            if (next != NULL && next->type == GL_AST_CONS) {
                gl_ast_node_t *cell = malloc(sizeof(gl_ast_node_t));
                assert(cell);
                dst->value.cons.cdr = cell;
                dst = cell;
                cur = next;
            } else {
                dst->value.cons.cdr = gl_ast_copy(next);
                break;
            }
        }
        return res;
    }

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
    case GL_AST_ARRAY: {
        gl_ast_array_init(&res->value.array, node->value.array.size, gl_ast_node_destroy_helper);
        for (size_t i = 0; i < node->value.array.size; ++i) {
            res->value.array.data[i] = gl_ast_copy(node->value.array.data[i]);
        }
        break;
    }
    case GL_AST_VECTOR: {
        gl_ast_vector_init(&res->value.vector, gl_ast_node_destroy_helper);
        for (size_t i = 0; i < node->value.vector.size; ++i) {
            gl_ast_vector_push_back(
                &res->value.vector,
                gl_ast_copy(*gl_ast_vector_at((gl_ast_vector_t *)&node->value.vector, i)));
        }
        break;
    }
    case GL_AST_NIL:
        break;
    case GL_AST_CONS:
        break;
    }
    return res;
}

void gl_ast_destroy(gl_ast_node_t *node) {
    if (!node) {
        return;
    }

    if (node->type == GL_AST_CONS) {
        // Free the spine iteratively; nested cars are destroyed recursively.
        gl_ast_node_t *cur = node;
        while (cur != NULL && cur->type == GL_AST_CONS) {
            gl_ast_node_t *next = cur->value.cons.cdr;
            gl_ast_destroy(cur->value.cons.car);
            free(cur);
            cur = next;
        }
        gl_ast_destroy(cur);
        return;
    }

    switch (node->type) {
    case GL_AST_ARRAY:
        gl_ast_array_destroy(&node->value.array);
        break;
    case GL_AST_VECTOR:
        gl_ast_vector_destroy(&node->value.vector);
        break;
    case GL_AST_SYMBOL:
    case GL_AST_STRING:
        free(node->value.symbol);
        break;
    default:
        // INT, FLOAT, NIL - no dynamic memory to free per node.
        break;
    }

    free(node);
}
