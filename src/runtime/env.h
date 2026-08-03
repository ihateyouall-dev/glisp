#pragma once

#include "utils/hashmap.h"
#include "value.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DECLARE(gl_value_t *, gl_value_table)

typedef struct gl_env_t {
    gl_value_table_t *variables;
    gl_value_table_t *functions;
    struct gl_env_t *parent;
} gl_env_t;

gl_env_t *gl_env_create(gl_env_t *parent);

void gl_env_destroy(gl_env_t *env);

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val);

void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val);

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym);

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym);
