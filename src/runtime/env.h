#pragma once

/**
 * @file env.h
 * @brief Lexical environment frames and symbol lookup.
 */

#include "utils/hashmap.h"
#include "value.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DECLARE(gl_value_t *, gl_value_table)

/** A lexical environment frame. */
typedef struct gl_env_t {
    gl_value_table_t *variables; /**< Variable bindings. */
    gl_value_table_t *functions; /**< Function bindings (separate namespace). */
    gl_value_table_t *constants; /**< Protected names; NULL until first defconst. */
    gl_value_t *parent;          /**< Enclosing GL_VAL_ENV, or NULL for the global frame. */
} gl_env_t;

/** The global environment value. */
extern gl_value_t *gl_global_env;

/** Allocates a frame whose parent is @p parent (a GL_VAL_ENV, or NULL). */
gl_env_t *gl_make_env(gl_value_t *parent);

/** Builds the initial global frame with builtins, special forms, and constants. */
gl_env_t *gl_make_global_env(void);

/** Creates the global frame and stores it in gl_global_env. */
void gl_init_global_env(void);

/** Frees a frame and its hash tables (bound values are GC-owned). */
void gl_env_destroy(gl_env_t *env);

/** Inserts or replaces a variable binding in @p env. */
void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val);

/** Inserts or replaces a function binding; asserts the value is callable. */
void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val);

/** Defines an immutable constant in @p env. */
void gl_env_set_const(gl_env_t *env, const char *sym, gl_value_t *val);

/** Returns non-zero when @p sym is a constant in @p env or any parent. */
int gl_env_is_const(gl_env_t *env, const char *sym);

/** Looks up a variable, walking parent frames; returns NULL when unbound. */
gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym);

/** Looks up a function, walking parent frames; returns NULL when unbound. */
gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym);
