#pragma once

#include "ast.h"
#include "env.h"
#include "value.h"
#include "vector.h"

typedef struct {
    char *func_name;
    gl_ast_node_t *call_node;
    gl_env_t *env;
} gl_call_frame_t;

VECTOR_DECLARE(gl_call_frame_t, __gl_call_stack)

extern __gl_call_stack_t gl_call_stack;
extern int gl_call_stack_initialized;

void gl_init_call_stack(void);

gl_env_t *gl_current_env(void);

void gl_call_stack_push_call(gl_function_t *function, gl_ast_node_t *function_node);

void gl_call_stack_pop_call(void);

void gl_call_stack_print_stacktrace();

gl_call_frame_t gl_call_stack_current_frame();
