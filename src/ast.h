#pragma once

/**
 * @file ast.h
 * @brief Tagged AST node representation shared by the parser and evaluator.
 */

#include "utils/array.h"
#include "utils/cons.h"
#include "utils/location.h"
#include "utils/vector.h"

#include <stddef.h>
#include <stdint.h>

/** Kinds of AST nodes produced by the parser. */
typedef enum {
    GL_AST_INT,
    GL_AST_FLOAT,
    GL_AST_SYMBOL,
    GL_AST_CONS,
    GL_AST_STRING,
    GL_AST_ARRAY,
    GL_AST_VECTOR,
    GL_AST_NIL
} gl_ast_node_type_t;

struct gl_ast_node_t;

CONS_DECLARE(struct gl_ast_node_t *, gl_ast_cons);

ARRAY_DECLARE(struct gl_ast_node_t *, gl_ast_array);
VECTOR_DECLARE(struct gl_ast_node_t *, gl_ast_vector);

/** A parsed expression with its source location. */
typedef struct gl_ast_node_t {
    gl_ast_node_type_t type; /**< Active union member. */
    gl_location_t location;  /**< Source position for diagnostics. */

    union {
        int64_t integral;
        long double floating;
        char *symbol;
        char *string;

        gl_ast_cons_t cons;
        gl_ast_array_t array;
        gl_ast_vector_t vector;
    } value;
} gl_ast_node_t;

/** Destroys a node through a pointer-to-pointer and clears the pointer. */
void gl_ast_node_destroy_helper(gl_ast_node_t **node);

/** Returns the car of the @p n-th cons cell in @p cons, or NULL if too short. */
gl_ast_node_t *gl_ast_cons_nth_car(gl_ast_node_t *cons, size_t n);

/** Returns the cdr of the @p n-th cons cell in @p cons, or NULL if too short. */
gl_ast_node_t *gl_ast_cons_nth_cdr(gl_ast_node_t *cons, size_t n);

/** Counts the elements of a proper cons list. */
size_t gl_ast_cons_length(gl_ast_node_t *cons);

/** Wraps @p node in a (quote ...) form. */
gl_ast_node_t *gl_ast_make_quote(gl_ast_node_t *node);

/** Wraps @p node in a (function ...) form. */
gl_ast_node_t *gl_ast_make_funquote(gl_ast_node_t *node);

gl_ast_node_t *gl_ast_make_int(int64_t num, gl_location_t location);
gl_ast_node_t *gl_ast_make_float(long double num, gl_location_t location);
gl_ast_node_t *gl_ast_make_symbol(const char *sym, gl_location_t location);
gl_ast_node_t *gl_ast_make_cons(gl_ast_node_t *car, gl_ast_node_t *cdr, gl_location_t location);
gl_ast_node_t *gl_ast_make_string(char *str, gl_location_t location);
gl_ast_node_t *gl_ast_make_array(gl_ast_array_t array, gl_location_t location);
gl_ast_node_t *gl_ast_make_vector(gl_ast_vector_t vector, gl_location_t location);
gl_ast_node_t *gl_ast_make_nil(gl_location_t location);
/** Deep-copies an AST tree. The caller owns the copy. */
gl_ast_node_t *gl_ast_copy(gl_ast_node_t *node);

/** Frees an AST tree. */
void gl_ast_destroy(gl_ast_node_t *node);
