/**
 * @file callstack.c
 * @brief Call frames, current-environment resolution, and stack traces.
 */

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

// A call frame gets a fresh child environment of the function's closure.
void gl_call_stack_push_call(gl_function_t *function, gl_ast_node_t *function_node) {
    gl_value_t *local_env = gl_value_make_env(gl_make_env(function->closure));
    __gl_call_stack_push_back(&gl_call_stack, (gl_call_frame_t){.call_node = function_node,
                                                                .func_name = function->name,
                                                                .env = local_env});
}

void gl_call_stack_pop_call(void) { __gl_call_stack_pop_back(&gl_call_stack); }

void gl_call_stack_push_env(gl_value_t *env, gl_ast_node_t *call_node, const char *name) {
    gl_init_call_stack();
    __gl_call_stack_push_back(
        &gl_call_stack,
        (gl_call_frame_t){.call_node = call_node, .func_name = (char *)name, .env = env});
}

void gl_call_stack_pop_env(void) {
    if (gl_call_stack_initialized && gl_call_stack.size > 0) {
        __gl_call_stack_pop_back(&gl_call_stack);
    }
}

static void __gl_print_call_frame(gl_call_frame_t frame) {
    fprintf(stderr, "  at - %s:%zu:%zu (%s)\n", frame.call_node->location.unit_name,
            frame.call_node->location.line, frame.call_node->location.column, frame.func_name);
}

// Pops and prints every frame from top to bottom. The stack is left empty.
void gl_call_stack_print_stacktrace() {
    // Unwinding calls this once per failed call, but the first call already
    // emptied the stack.
    if (!gl_call_stack_initialized) {
        return;
    }

    // Runaway recursion would otherwise print thousands of identical frames.
    const size_t limit = 20;
    size_t total = gl_call_stack.size;
    size_t printed = 0;

    for (size_t i = total; i > 0; --i) {
        gl_call_frame_t frame = __gl_call_stack_pop_back(&gl_call_stack);
        if (frame.call_node == NULL) {
            continue;
        }
        if (printed < limit) {
            __gl_print_call_frame(frame);
            gl_diagnostic_show_frame(frame.call_node->location);
            ++printed;
        }
    }

    if (total > printed) {
        fprintf(stderr, "... %zu frames omitted\n", total - printed);
    }

    __gl_call_stack_destroy(&gl_call_stack);
    gl_call_stack_initialized = 0;
}

gl_call_frame_t gl_call_stack_current_frame() {
    return *__gl_call_stack_at(&gl_call_stack, gl_call_stack.size - 1);
}

// The current environment is the top frame, or the global environment.
gl_value_t *gl_current_env(void) {
    gl_value_t *res = gl_global_env;
    if (gl_call_stack.size > 0) {
        res = gl_call_stack_current_frame().env;
    }
    return res;
}

void gl_call_stack_cleanup(void) {
    if (gl_call_stack_initialized) {
        __gl_call_stack_destroy(&gl_call_stack);
        gl_call_stack_initialized = 0;
    }
}
