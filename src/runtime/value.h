#pragma once

/**
 * @file value.h
 * @brief The universal runtime value type and its constructors.
 */

#include "ast.h"
#include "utils/array.h"
#include "utils/cons.h"
#include "utils/location.h"
#include "utils/vector.h"

#include <stdint.h>
#include <stdio.h>

/** Tags for every runtime value kind. */
typedef enum {
    GL_VAL_NIL,
    GL_VAL_INT,
    GL_VAL_FLOAT,
    GL_VAL_SYMBOL,
    GL_VAL_FUNCTION,
    GL_VAL_BUILTIN,
    GL_VAL_SPFORM,
    GL_VAL_CONS,
    GL_VAL_STRING,
    GL_VAL_ENV,
    GL_VAL_ARRAY,
    GL_VAL_VECTOR,
    GL_VAL_TYPEID,
    GL_VAL_FILE
} gl_value_type_t;

/**
 * A garbage-collected runtime value.
 *
 * Values are linked into a global heap list through @ref next_in_heap. The
 * payload layout depends on @ref type and is accessed through the gl_value_get_*
 * and gl_value_set_* helpers.
 */
typedef struct gl_value_t {
    gl_value_type_t type;            /**< Active value kind. */
    void *val;                       /**< Payload; meaning depends on @ref type. */
    int gc_marked;                   /**< Mark bit used by the collector. */
    struct gl_value_t *next_in_heap; /**< Intrusive heap list link. */
} gl_value_t;

/** Singleton nil value; never collected. */
extern gl_value_t *gl_nil;

/** Singleton true value; never collected. */
extern gl_value_t *gl_t;

CONS_DECLARE(gl_value_t *, gl_value_cons);
ARRAY_DECLARE(gl_value_t *, gl_value_array);
VECTOR_DECLARE(gl_value_t *, gl_value_vector);

/** Frees a value and its payload. Intended for the collector and shutdown. */
void gl_value_destroy(gl_value_t **val);

/** Deep-copies a value (lists are copied recursively). The copy is registered. */
gl_value_t *gl_value_copy(gl_value_t *val);

gl_value_t *gl_value_make_int(int64_t num);

gl_value_t *gl_value_make_float(long double num);

gl_value_t *gl_value_make_symbol(const char *sym);

gl_value_t *gl_value_make_cons(gl_ast_cons_t cons);

gl_value_t *gl_value_make_cons_raw(gl_value_t *car, gl_value_t *cdr);

gl_value_t *gl_value_make_string(char *str);

gl_value_t *gl_value_make_nil(void);

gl_value_t *gl_value_make_bool(int b);

gl_value_t *gl_value_make_typeid(gl_value_type_t type);

// If multiple permissions provided, just sum up the modes.
// Like "rw" = GL_FILE_READ | GL_FILE_WRITE = 3
typedef enum { GL_FILE_READ = 1, GL_FILE_WRITE = 2, GL_FILE_APPEND = 4 } gl_file_mode_t;

/** Shared file payload. Copies of a file value share this structure. */
typedef struct {
    FILE *handle;        /**< stdio handle, or NULL when closed. */
    char *name;          /**< File name used in diagnostics and printing. */
    int is_open;         /**< Non-zero while the handle is open. */
    gl_file_mode_t mode; /**< Mode bitmask the file was opened with. */
    int ref_count;       /**< Number of value copies referencing this payload. */
} gl_file_t;

gl_value_t *gl_value_make_file(FILE *handle, char *name, int is_open, gl_file_mode_t mode);

typedef struct gl_env_t gl_env_t;

// Used for error handling
/** One evaluated argument plus the source location of its expression. */
typedef struct {
    gl_value_t *val;        /**< Evaluated argument value. */
    gl_location_t location; /**< Location of the argument expression. */
} gl_function_arg_t;

ARRAY_DECLARE(gl_function_arg_t *, gl_function_args)

void gl_function_arg_free(gl_function_arg_t **arg);

/** Signature of a native (C) function. */
typedef gl_value_t *(*gl_builtin_t)(gl_location_t called_at, gl_function_args_t *);

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin);

/** Parameter categories in a lambda list. */
typedef enum { GL_PARAM_REQUIRED, GL_PARAM_OPTIONAL, GL_PARAM_REST } gl_param_kind_t;

/** One parameter of a user-defined function. */
typedef struct {
    char *name;                 /**< Binding name. */
    gl_param_kind_t kind;       /**< Required, optional, or rest. */
    gl_ast_node_t *default_val; /**< Unevaluated default for &optional, or NULL. */
} gl_function_param_t;

ARRAY_DECLARE(gl_function_param_t *, gl_function_params)

void gl_function_param_free(gl_function_param_t **param);
gl_function_param_t *gl_function_param_make(const char *name, gl_param_kind_t kind,
                                            gl_ast_node_t *default_val);
gl_function_param_t *gl_function_param_copy(const gl_function_param_t *param);

/** A user-defined function (closure). */
typedef struct {
    gl_ast_node_t *tree;          /**< Owned body AST (a cons list of expressions). */
    gl_function_params_t *params; /**< Owned parameter list. */
    gl_value_t *closure;          /**< Defining environment captured at creation. */
    char *name;                   /**< Name for diagnostics and printing. */
    size_t min_args;              /**< Minimum accepted argument count. */
    size_t max_args;              /**< Maximum accepted argument count (ignored with &rest). */
    int has_rest;                 /**< Non-zero when the function has &rest. */
} gl_function_t;

gl_value_t *gl_value_make_function(const char *name, gl_ast_node_t *tree,
                                   gl_function_params_t *params, gl_value_t *closure);

/** Signature of a special form; arguments are unevaluated AST nodes. */
typedef gl_value_t *(*gl_special_form_t)(gl_env_t *, gl_ast_node_t *);

gl_value_t *gl_value_make_specform(gl_special_form_t spform);

gl_value_t *gl_value_make_env(gl_env_t *env);

gl_value_t *gl_value_make_array(size_t size);
gl_value_t *gl_value_make_array_from(gl_value_array_t *arr);

gl_value_t *gl_value_make_vector(void);
gl_value_t *gl_value_make_vector_from(gl_value_vector_t *vec);

/** Prints a value's textual representation to @p file. */
void gl_value_fprint(FILE *file, gl_value_t *val);

/** Prints a value to stdout. */
void gl_value_print(gl_value_t *val);

/** Returns a heap-allocated textual representation; the caller frees it. */
char *gl_value_to_string(gl_value_t *val);

int64_t gl_value_get_int(gl_value_t *val);

long double gl_value_get_float(gl_value_t *val);

char *gl_value_get_symbol(gl_value_t *val);

char *gl_value_get_string(gl_value_t *val);

gl_value_cons_t *gl_value_get_cons(gl_value_t *val);

gl_value_array_t *gl_value_get_array(gl_value_t *val);

gl_value_vector_t *gl_value_get_vector(gl_value_t *val);

gl_value_type_t gl_value_get_typeid(gl_value_t *val);

gl_file_t *gl_value_get_file(gl_value_t *val);

gl_builtin_t gl_value_get_builtin(gl_value_t *val);

gl_function_t *gl_value_get_function(gl_value_t *val);

gl_special_form_t gl_value_get_specform(gl_value_t *val);

int gl_value_get_bool(gl_value_t *val);

void gl_value_set_int(gl_value_t *val, int64_t num);

void gl_value_set_float(gl_value_t *val, long double num);

void gl_value_set_symbol(gl_value_t *val, char *sym);

void gl_value_set_builtin(gl_value_t *val, gl_builtin_t builtin);

void gl_value_set_function(gl_value_t *val, gl_function_t *func);

void gl_value_set_specform(gl_value_t *val, gl_special_form_t specform);

/**
 * Structural comparison used by the language's equality predicate.
 *
 * @return 0 if equal, a positive value if @p lhs sorts after @p rhs, or a
 *         negative value otherwise. Numbers are compared numerically (mixed
 *         int/float allowed); symbols and strings by content; lists, arrays,
 *         and vectors element-wise.
 */
int gl_value_compare(gl_value_t *lhs, gl_value_t *rhs);
