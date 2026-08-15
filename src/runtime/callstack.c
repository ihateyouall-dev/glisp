#include "callstack.h"
#include "ast.h"
#include "diagnostics.h"
#include "env.h"
#include "value.h"
#include <stdio.h>

VECTOR_DEFINE(gl_call_frame_t, __gl_call_stack)

__gl_call_stack_t gl_call_stack;
int gl_call_stack_initialized = 0;

static void __gl_call_frame_destroy(gl_call_frame_t *unused) {}

void gl_init_call_stack() {
    if (!gl_call_stack_initialized) {
        __gl_call_stack_init(&gl_call_stack, __gl_call_frame_destroy);
        gl_call_stack_initialized = 1;
    }
}

void gl_call_stack_push_call(gl_function_t *function, gl_ast_node_t *function_node) {
    gl_env_t *local_env = gl_make_env(gl_current_env());
    __gl_call_stack_push_back(&gl_call_stack, (gl_call_frame_t){.call_node = function_node,
                                                                .func_name = function->name,
                                                                .env = local_env});
}

void gl_call_stack_pop_call(void) { __gl_call_stack_pop_back(&gl_call_stack); }

static void __gl_print_call_frame(gl_call_frame_t frame) {
    fprintf(stderr, "  at - %s:%zu:%zu (%s)\n", frame.call_node->location.unit_name,
            frame.call_node->location.line, frame.call_node->location.column, frame.func_name);
}

void gl_call_stack_print_stacktrace() {
    for (size_t i = gl_call_stack.size; i > 0; --i) {
        gl_call_frame_t frame = __gl_call_stack_pop_back(&gl_call_stack);
        __gl_print_call_frame(frame);
        gl_diagnostic_show_frame(frame.call_node->location);
    }
    __gl_call_stack_destroy(&gl_call_stack);
    gl_call_stack_initialized = 0;
}

gl_call_frame_t gl_call_stack_current_frame() {
    return *__gl_call_stack_at(&gl_call_stack, gl_call_stack.size - 1);
}

gl_env_t *gl_current_env(void) {
    gl_env_t *res = gl_global_env;
    if (gl_call_stack.size > 0) {
        res = gl_call_stack_current_frame().env;
    }
    return res;
}
