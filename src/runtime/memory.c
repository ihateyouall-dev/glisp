#include "memory.h"

gl_memory_heap_t *gl_memory_heap = NULL;

void gl_init_memory_heap(void) {
    gl_memory_heap = malloc(sizeof(gl_memory_heap_t));
    gl_memory_heap->allocated = 0;
}

gl_object_t *gl_register_object(gl_obj_type_t type, void *raw) {
    if (!raw) {
        return NULL;
    }

    if (gl_memory_heap == NULL) {
        gl_init_memory_heap();
    }

    gl_object_t *new = malloc(sizeof(gl_object_t));
    new->type = type;

    if (type == GL_OBJ_ENV) {
        new->ptr.env = raw;
    } else {
        new->ptr.value = raw;
    }

    new->marked = 0;

    new->next = gl_memory_heap->head;

    gl_memory_heap->head = new;
    ++gl_memory_heap->allocated;

    return new;
}

void gl_object_destroy(gl_object_t **obj) {
    if (!obj || !*obj) {
        return;
    }

    free(*obj);
    *obj = NULL;
}

gl_object_t *gl_object_copy(gl_object_t *obj) {
    gl_object_t *res = malloc(sizeof(gl_object_t));

    res->type = obj->type;
    res->ptr = obj->ptr;
    res->next = obj->next;
    res->marked = obj->marked;

    return res;
}

gl_object_t *gl_register_env(gl_env_t *env) { return gl_register_object(GL_OBJ_ENV, env); }

gl_object_t *gl_register_value(gl_value_t *val) { return gl_register_object(GL_OBJ_VALUE, val); }
