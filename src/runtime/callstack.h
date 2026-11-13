#pragma once

/**
 * @file callstack.h
 * @brief Call-stack frames and environment lookup.
 */

#include "ast.h"
#include "value.h"
#include "vector.h"

/** One active call or lexical scope. */
typedef struct {
    char *func_name;          /**< Display name for stack traces. */
    gl_ast_node_t *call_node; /**< Call site (borrowed). */
    gl_value_t *env;          /**< GL_VAL_ENV frame for this call. */
} gl_call_frame_t;

VECTOR_DECLARE(gl_call_frame_t, __gl_call_stack)

extern __gl_call_stack_t gl_call_stack;
extern int gl_call_stack_initialized;

/** Lazily initializes the global call stack. */
void gl_init_call_stack(void);

/** Returns the environment of the top frame, or the global environment. */
gl_value_t *gl_current_env(void);

/** Pushes a frame with a fresh child environment of the function's closure. */
void gl_call_stack_push_call(gl_function_t *function, gl_ast_node_t *function_node);

/** Pops the top function-call frame. */
void gl_call_stack_pop_call(void);

/** Pushes an explicit lexical frame (used by let, let*, and for). */
void gl_call_stack_push_env(gl_value_t *env, gl_ast_node_t *call_node, const char *name);

/** Pops the top explicit lexical frame. */
void gl_call_stack_pop_env(void);

/** Prints a stack trace and clears the call stack. */
void gl_call_stack_print_stacktrace();

/** Returns the top frame; the stack must be non-empty. */
gl_call_frame_t gl_call_stack_current_frame();

/** Releases the call-stack storage. */
void gl_call_stack_cleanup(void);
