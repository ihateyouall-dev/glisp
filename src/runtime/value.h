#pragma once

#include "ast.h"
#include "location.h"
#include "utils/array.h"
#include "utils/cons.h"
#include <stdint.h>

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
    GL_VAL_ENV
} gl_value_type_t;

typedef struct gl_value_t {
    gl_value_type_t type;
    void *val;
    int gc_marked;
    struct gl_value_t *next_in_heap;
} gl_value_t;

CONS_DECLARE(gl_value_t *, gl_value_cons)

void gl_value_destroy(gl_value_t **val);

gl_value_t *gl_value_copy(gl_value_t *val);

gl_value_t *gl_value_make_int(int64_t num);

gl_value_t *gl_value_make_float(long double num);

gl_value_t *gl_value_make_symbol(const char *sym);

gl_value_t *gl_value_make_cons(gl_ast_cons_t cons);

gl_value_t *gl_value_make_string(char *str);

gl_value_t *gl_value_make_nil(void);

gl_value_t *gl_value_make_bool(int b);

typedef struct gl_env_t gl_env_t;

// Used for error handling
typedef struct {
    gl_value_t *val;
    gl_location_t location;
} gl_function_arg_t;

ARRAY_DECLARE(gl_function_arg_t *, gl_function_args)

typedef gl_value_t *(*gl_builtin_t)(gl_location_t called_at, gl_function_args_t *);

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin);

ARRAY_DECLARE(char *, gl_function_params)

void gl_function_param_free(char **param);

typedef struct {
    gl_ast_node_t *tree;
    gl_function_params_t *params;
    gl_env_t *closure;
    char *name;
} gl_function_t;

gl_value_t *gl_value_make_function(char *name, gl_ast_node_t *tree, gl_function_params_t *params,
                                   gl_env_t *closure);

typedef gl_value_t *(*gl_special_form_t)(gl_env_t *, gl_ast_node_t *);

gl_value_t *gl_value_make_specform(gl_special_form_t spform);

gl_value_t *gl_value_make_env(gl_env_t *env);

void gl_value_print(gl_value_t *val);

int64_t gl_value_get_int(gl_value_t *val);

long double gl_value_get_float(gl_value_t *val);

char *gl_value_get_symbol(gl_value_t *val);

gl_value_cons_t *gl_value_get_cons(gl_value_t *val);

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

// Returns 0 if two values are equal, otherwise returns >0 or <0
int gl_value_compare(gl_value_t *lhs, gl_value_t *rhs);
