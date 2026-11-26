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

/**
 * One parameter bound in a call frame.
 *
 * The name is borrowed from the function's parameter list, which lives as long
 * as the function value that owns the frame. Parameters are kept in a small
 * dense array instead of the frame's variable table: a call binds a handful of
 * names, and a linear scan beats hashing plus a key copy at that size.
 */
typedef struct {
    const char *name;
    gl_value_t *value;
} gl_env_param_t;

/** A lexical environment frame. */
typedef struct gl_env_t {
    gl_value_table_t *variables; /**< Variable bindings. */
    gl_value_table_t *functions; /**< Function bindings (separate namespace). */
    gl_value_table_t *constants; /**< Protected names; NULL until first defconst. */
    gl_value_t *parent;          /**< Enclosing GL_VAL_ENV, or NULL for the global frame. */
    gl_env_param_t *params;      /**< Call parameters; NULL until first binding. */
    size_t param_count;          /**< Number of bound parameters. */
    size_t param_capacity;       /**< Allocated parameter slots. */
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

/**
 * Binds a function parameter in @p env. The name is borrowed, not copied, and
 * must outlive the frame.
 */
void gl_env_bind_param(gl_env_t *env, const char *sym, gl_value_t *val);

/** Returns non-zero when @p sym is bound in @p env itself, ignoring parents. */
int gl_env_has_local_var(gl_env_t *env, const char *sym);

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

/**
 * Resolves a callable name using the nearest lexical binding.
 *
 * Frames are searched from innermost to outermost. Within one frame a function
 * binding wins over a variable binding, but an inner variable shadows an outer
 * function of the same name.
 */
gl_value_t *gl_env_get_callable(gl_env_t *env, const char *sym);

/**
 * Returns the bound name closest to @p sym in the function and variable tables
 * of @p env and
 * its parents, or NULL when nothing is close enough.
 *
 * The result is heap-allocated and owned
 * by the caller. It is used to explain a
 * failed lookup: "undefined function 'printn' (did you
 * mean 'println'?)".
 */
char *gl_env_suggest_name(gl_env_t *env, const char *sym);
