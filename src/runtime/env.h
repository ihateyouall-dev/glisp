#pragma once

#include "../utils/hashmap.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    GL_VAL_NIL,
    GL_VAL_BOOLEAN,
    GL_VAL_INT,
    GL_VAL_FLOAT,
    GL_VAL_SYMBOL,
    GL_VAL_FUNCTION,
    GL_VAL_BUILTIN,
    GL_VAL_CONS
} gl_value_type_t;

typedef struct gl_value_t {
    gl_value_type_t type;
    void *val;
} gl_value_t;

HASHMAP_DECLARE(gl_value_t, value_table)

typedef struct gl_env_t {
    value_table_t *variables;
    value_table_t *functions;
    struct gl_env_t *parent;
} gl_env_t;

gl_env_t *gl_env_create(gl_env_t *parent);

void gl_env_destroy(gl_env_t *env);

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t val);

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym);

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym);

typedef gl_value_t (*gl_builtin)(gl_env_t *env, size_t argc, gl_value_t *argv);

#define GL_BUILTIN(Name) gl_value_t gl_builtin_##Name(gl_env_t *env, size_t argc, gl_value_t *argv)

GL_BUILTIN(exit);

GL_BUILTIN(funcall);
