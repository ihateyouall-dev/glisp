#pragma once

#include "ast.h"
#include "utils/cons.h"
#include "utils/hashmap.h"
#include "utils/strdup.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    GL_VAL_NIL,
    GL_VAL_INT,
    GL_VAL_FLOAT,
    GL_VAL_SYMBOL,
    GL_VAL_FUNCTION,
    GL_VAL_BUILTIN,
    GL_VAL_CONS
} gl_value_type_t;

typedef struct {
    gl_value_type_t type;
    void *val;
} gl_value_t;

CONS_DECLARE(gl_value_t *, gl_value_cons)

void gl_value_destroy(gl_value_t **val);

gl_value_t *gl_value_make_int(int64_t num);

gl_value_t *gl_value_make_float(long double num);

gl_value_t *gl_value_make_symbol(const char *sym);

gl_value_t *gl_value_make_cons(gl_ast_cons_t cons);

gl_value_t *gl_value_make_nil(void);

HASHMAP_DECLARE(gl_value_t *, gl_value_table)

typedef struct gl_env_t {
    gl_value_table_t *variables;
    gl_value_table_t *functions;
    struct gl_env_t *parent;
} gl_env_t;

typedef struct {
    gl_ast_node_t *tree;
    char **args;
    gl_env_t *closure;
} gl_function_t;

gl_value_t *gl_value_make_function(gl_ast_node_t *tree, char **args, gl_env_t *closure);

gl_env_t *gl_env_create(gl_env_t *parent);

void gl_env_destroy(gl_env_t *env);

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val);

void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val);

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym);

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym);

typedef gl_value_t (*gl_builtin_t)(gl_env_t *env, size_t argc, gl_value_t *argv);

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin);

#define GL_BUILTIN(Name) gl_value_t gl_builtin_##Name(gl_env_t *env, size_t argc, gl_value_t *argv)

GL_BUILTIN(exit);

GL_BUILTIN(add);
