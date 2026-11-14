/**
 * @file memory.c
 * @brief Mark-and-sweep garbage collector and temporary roots.
 */

#include "memory.h"

#include "callstack.h"
#include "env.h"
#include "value.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

gl_memory_heap_t *gl_memory_heap = NULL;

const size_t GL_GC_THRESHOLD = 5000;
int gl_gc_enabled = 1;

// Collection runs when the heap exceeds this threshold. After a collection the
// threshold is raised to twice the live set so that a large but stable heap
// does not trigger a collection at every safe point (which would be O(n^2)).
static size_t gl_gc_threshold = GL_GC_THRESHOLD;

void gl_init_memory_heap(void) {
    gl_memory_heap = malloc(sizeof(gl_memory_heap_t));
    gl_memory_heap->allocated = 0;
    gl_memory_heap->head = NULL;
    gl_gc_threshold = GL_GC_THRESHOLD;
}

static gl_value_vector_t gl_temp_roots;

// Temporary roots form a stack: mark before pushing, release back to the mark
// once the values are reachable through Lisp structures again.
size_t gl_gc_root_mark(void) { return gl_temp_roots.size; }

void gl_gc_push_root(gl_value_t *val) {
    if (val == NULL || val == gl_nil || val == gl_t) {
        return;
    }

    gl_value_vector_push_back(&gl_temp_roots, val);
}

void gl_gc_roots_release(size_t mark) {
    if (mark < gl_temp_roots.size) {
        gl_value_vector_resize(&gl_temp_roots, mark);
    }
}

void gl_gc_maybe_collect(void) {
    if (gl_gc_enabled && gl_memory_heap != NULL && gl_memory_heap->allocated > gl_gc_threshold) {
        gl_gc_collect();
    }
}

// Registration only links the value; collection is deferred to a safe point.
void gl_register_value(gl_value_t *val) {
    if (!val || val == gl_nil || val == gl_t) {
        return;
    }

    if (gl_memory_heap == NULL) {
        gl_init_memory_heap();
    }

    val->gc_marked = 0;

    val->next_in_heap = gl_memory_heap->head;

    gl_memory_heap->head = val;
    ++gl_memory_heap->allocated;
}

static void __gl_trace_env_raw(gl_env_t *raw);
static void __gl_trace_environment(gl_value_t *env);

// Marks a value and recursively traces its payload.
static void __gl_trace_value(gl_value_t *val) {
    if (val == NULL || val == gl_nil || val == gl_t || val->gc_marked) {
        return;
    }

    if (val->type == GL_VAL_ENV) {
        __gl_trace_environment(val);
        return;
    }

    if (val->type == GL_VAL_CONS) {
        // Walk the spine iteratively: long flat lists must not recurse once per
        // cell. Each car may still recurse into nested structures.
        gl_value_t *cur = val;
        while (cur != NULL && cur->type == GL_VAL_CONS) {
            cur->gc_marked = 1;
            gl_value_cons_t *cons = cur->val;
            if (cons == NULL) {
                break;
            }
            __gl_trace_value(cons->car);
            cur = cons->cdr;
            if (cur != NULL && cur->gc_marked) {
                return;
            }
        }
        __gl_trace_value(cur);
        return;
    }

    val->gc_marked = 1;

    switch (val->type) {
    case GL_VAL_FUNCTION: {
        gl_function_t *func = val->val;
        if (func && func->closure) {
            __gl_trace_value(func->closure);
        }
        break;
    }
    case GL_VAL_ARRAY: {
        gl_value_array_t *arr = val->val;
        if (arr && arr->data) {
            for (size_t i = 0; i < arr->size; ++i) {
                __gl_trace_value(arr->data[i]);
            }
        }
        break;
    }
    case GL_VAL_VECTOR: {
        gl_value_vector_t *vec = val->val;
        if (vec && vec->data) {
            for (size_t i = 0; i < vec->size; ++i) {
                __gl_trace_value(*gl_value_vector_at(vec, i));
            }
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

        for (size_t j = 0; j < bucket->size; ++j) {
            gl_value_tableEntry_t *val = gl_value_tableBucket_at(bucket, j);
            if (val && val->value) {
                __gl_trace_value(val->value);
            }
            ++count;
        }
    }
}

static void __gl_trace_env_raw(gl_env_t *raw) {
    if (raw == NULL) {
        return;
    }
    if (raw->variables) {
        __gl_trace_value_table(raw->variables);
    }
    if (raw->functions) {
        __gl_trace_value_table(raw->functions);
    }
    if (raw->constants) {
        __gl_trace_value_table(raw->constants);
    }

    if (raw->parent != NULL) {
        __gl_trace_environment(raw->parent);
    }
}

static void __gl_trace_environment(gl_value_t *env) {
    if (env == NULL || env->gc_marked) {
        return;
    }
    env->gc_marked = 1;
    __gl_trace_env_raw(env->val);
}

// Mark-and-sweep: clear marks, trace roots, then free unmarked nodes.
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

    for (size_t i = 0; i < gl_temp_roots.size; ++i) {
        __gl_trace_value(gl_temp_roots.data[i]);
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

    size_t live = gl_memory_heap->allocated;
    gl_gc_threshold =
        live > GL_GC_THRESHOLD ? (live > SIZE_MAX / 2 ? SIZE_MAX : live * 2) : GL_GC_THRESHOLD;
}

void gl_destroy_memory_heap(void) {
    if (gl_memory_heap == NULL) {
        return;
    }

    gl_value_t *current = gl_memory_heap->head;
    while (current != NULL) {
        gl_value_t *next = current->next_in_heap;
        gl_value_destroy(&current);
        current = next;
    }

    free(gl_memory_heap);
    gl_memory_heap = NULL;
    gl_global_env = NULL;
    gl_gc_enabled = 1;

    gl_value_vector_destroy(&gl_temp_roots);

    if (gl_nil) {
        free(gl_nil);
        gl_nil = NULL;
    }
    if (gl_t) {
        free(gl_t);
        gl_t = NULL;
    }

    gl_destroy_sources();
}

typedef struct gl_source_entry_t {
    char *buf;
    char *unit_name;
    struct gl_source_entry_t *next;
} gl_source_entry_t;

static gl_source_entry_t *gl_source_list = NULL;

void gl_register_source(char *buf, char *unit_name) {
    gl_source_entry_t *entry = malloc(sizeof(gl_source_entry_t));
    entry->buf = buf;
    entry->unit_name = unit_name;
    entry->next = gl_source_list;
    gl_source_list = entry;
}

void gl_destroy_sources(void) {
    gl_source_entry_t *curr = gl_source_list;
    while (curr) {
        gl_source_entry_t *next = curr->next;
        free(curr->buf);
        free(curr->unit_name);
        free(curr);
        curr = next;
    }
    gl_source_list = NULL;
}
