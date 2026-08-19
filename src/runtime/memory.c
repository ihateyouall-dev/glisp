#include "memory.h"
#include "callstack.h"
#include "env.h"
#include "value.h"
#include <stddef.h>

gl_memory_heap_t *gl_memory_heap = NULL;

void gl_init_memory_heap(void) {
    gl_memory_heap = malloc(sizeof(gl_memory_heap_t));
    gl_memory_heap->allocated = 0;
    gl_memory_heap->head = NULL;
}

const size_t GL_GC_THRESHOLD = 5000;

void gl_register_value(gl_value_t *val) {
    if (!val) {
        return;
    }

    if (gl_memory_heap == NULL) {
        gl_init_memory_heap();
    }

    val->gc_marked = 0;

    val->next_in_heap = gl_memory_heap->head;

    gl_memory_heap->head = val;
    ++gl_memory_heap->allocated;

    if (gl_memory_heap->allocated > GL_GC_THRESHOLD) {
        gl_gc_collect();
    }
}

static void __gl_trace_environment(gl_value_t *env);

static void __gl_trace_value(gl_value_t *val) {
    if (val == NULL || val->gc_marked) {
        return;
    }
    val->gc_marked = 1;

    switch (val->type) {
    case GL_VAL_CONS: {
        gl_value_cons_t *cons = val->val;
        if (cons) {
            __gl_trace_value(cons->car);
            __gl_trace_value(cons->cdr);
        }
        break;
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *func = val->val;
        if (func && func->closure) {
            __gl_trace_environment(func->closure);
        }
        break;
    }
    case GL_VAL_ENV: {
        if (val->val) {
            __gl_trace_environment(val);
        }
        break;
    }
    default:
        break;
    }
}

static void __gl_trace_value_table(gl_value_table_t *table) {
    if (table == NULL || table->buckets == NULL) {
        return;
    }
    for (size_t i = 0, count = 0; i < table->buckets_count && count < table->size; ++i) {
        gl_value_tableBucket_t *bucket = &table->buckets[i];

        if (bucket == NULL) {
            continue;
        }

        for (size_t j = 0; j < bucket->size; ++j) {
            gl_value_tableEntry_t *val = gl_value_tableBucket_at(bucket, j);
            if (val && val->value) {
                __gl_trace_value(val->value);
            }
            ++count;
        }
    }
}

static void __gl_trace_environment(gl_value_t *env) {
    if (env == NULL || env->gc_marked) {
        return;
    }
    env->gc_marked = 1;
    gl_env_t *raw = env->val;
    if (raw == NULL) {
        return;
    }
    if (raw->variables) {
        __gl_trace_value_table(raw->variables);
    }
    if (raw->functions) {
        __gl_trace_value_table(raw->functions);
    }

    if (raw->parent != NULL) {
        __gl_trace_environment(raw->parent);
    }
}

void gl_gc_collect(void) {
    if (gl_memory_heap == NULL) {
        return;
    }

    gl_value_t *current = gl_memory_heap->head;
    while (current != NULL) {
        current->gc_marked = 0;
        current = current->next_in_heap;
    }

    __gl_trace_environment(gl_global_env);

    if (gl_call_stack_initialized && gl_call_stack.size > 0) {
        for (size_t i = 0; i < gl_call_stack.size; ++i) {
            gl_call_frame_t *frame = __gl_call_stack_at(&gl_call_stack, i);
            if (frame && frame->env) {
                __gl_trace_environment(frame->env);
            }
        }
    }

    current = gl_memory_heap->head;
    gl_value_t *prev = NULL;

    while (current != NULL) {
        if (!current->gc_marked) {
            gl_value_t *next = current->next_in_heap;

            if (prev == NULL) {
                gl_memory_heap->head = next;
            } else {
                prev->next_in_heap = next;
            }

            gl_value_destroy(&current);
            --gl_memory_heap->allocated;

            current = next;
        } else {
            prev = current;
            current = current->next_in_heap;
        }
    }
}
