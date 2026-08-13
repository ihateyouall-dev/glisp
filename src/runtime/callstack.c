#include "callstack.h"
#include "ast.h"
#include "value.h"
#include <stdio.h>

VECTOR_DEFINE(__gl_call_frame_t, __gl_call_stack)

__gl_call_stack_t gl_call_stack;
static int __gl_call_stack_created = 0;

static void __gl_call_frame_destroy(__gl_call_frame_t *unused) {}

void gl_init_call_stack() {
    if (!__gl_call_stack_created) {
        __gl_call_stack_init(&gl_call_stack, __gl_call_frame_destroy);
        __gl_call_stack_created = 1;
    }
}

void gl_call_stack_push_call(gl_function_t *function, gl_ast_node_t *function_node) {
    __gl_call_stack_push_back(&gl_call_stack, (__gl_call_frame_t){.call_node = function_node,
                                                                  .func_name = function->name});
}

static void __gl_print_call_frame(__gl_call_frame_t frame) {
    fprintf(stderr, "  at - %s:%zu:%zu (%s)\n", frame.call_node->location.unit_name,
            frame.call_node->location.line, frame.call_node->location.column, frame.func_name);
}

void gl_call_stack_print_stacktrace() {
    for (size_t i = gl_call_stack.size; i > 0; --i) {
        __gl_print_call_frame(__gl_call_stack_pop_back(&gl_call_stack));
    }
    __gl_call_stack_destroy(&gl_call_stack);
    __gl_call_stack_created = 0;
}
