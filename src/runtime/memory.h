#pragma once

#include "env.h"
#include "value.h"

typedef enum { GL_OBJ_VALUE, GL_OBJ_ENV } gl_obj_type_t;

typedef struct gl_object_t {
    gl_obj_type_t type;
    union {
        gl_env_t *env;
        gl_value_t *value;
    } ptr;
    int marked;
    struct gl_object_t *next;
} gl_object_t;

typedef struct {
    gl_object_t *head;
    size_t allocated;
} gl_memory_heap_t;

extern gl_memory_heap_t *gl_memory_heap;

void gl_init_memory_heap(void);

gl_object_t *gl_register_object(gl_obj_type_t type, void *raw);

gl_object_t *gl_register_env(gl_env_t *env);

gl_object_t *gl_register_value(gl_value_t *val);

void gl_object_destroy(gl_object_t **obj);

gl_object_t *gl_object_copy(gl_object_t *obj);
