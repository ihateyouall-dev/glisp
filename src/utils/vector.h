#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define VECTOR_DECLARE(T, Name)                                                                    \
    typedef struct {                                                                               \
        T *data;                                                                                   \
        size_t size;                                                                               \
        size_t capacity;                                                                           \
        void (*destroy_fn)(T *);                                                                   \
    } Name##_t;                                                                                    \
    void Name##_init(Name##_t *vec, void (*destroy_fn)(T *));                                      \
    void Name##_push_back(Name##_t *vec, T val);                                                   \
    T *Name##_at(Name##_t *vec, size_t idx);                                                       \
    T Name##_pop_back(Name##_t *vec);                                                              \
    void Name##_destroy(Name##_t *vec);                                                            \
    void Name##_reserve(Name##_t *vec, size_t capacity);                                           \
    void Name##_resize(Name##_t *vec, size_t size);                                                \
    void Name##_clear(Name##_t *vec);                                                              \
    int Name##_empty(const Name##_t *vec);

#define VECTOR_DEFINE(T, Name)                                                                     \
    void Name##_init(Name##_t *vec, void (*destroy_fn)(T *)) {                                     \
        vec->capacity = 8;                                                                         \
        vec->data = malloc(vec->capacity * sizeof(T));                                             \
        vec->size = 0;                                                                             \
        vec->destroy_fn = destroy_fn;                                                              \
    }                                                                                              \
    static int __##Name##_realloc(Name##_t *vec, size_t capacity) {                                \
        T *tmp = realloc(vec->data, capacity * sizeof(T));                                         \
        if (tmp) {                                                                                 \
            vec->data = tmp;                                                                       \
            vec->capacity = capacity;                                                              \
            return 1;                                                                              \
        }                                                                                          \
        return 0;                                                                                  \
    }                                                                                              \
    void Name##_push_back(Name##_t *vec, T val) {                                                  \
        if (vec->size == vec->capacity) {                                                          \
            __##Name##_realloc(vec, vec->capacity * 2);                                            \
        }                                                                                          \
        vec->data[vec->size++] = val;                                                              \
    }                                                                                              \
    T *Name##_at(Name##_t *vec, size_t idx) {                                                      \
        assert(idx < vec->size);                                                                   \
        return &vec->data[idx];                                                                    \
    }                                                                                              \
    T Name##_pop_back(Name##_t *vec) {                                                             \
        assert(vec->size > 0);                                                                     \
        T res = vec->data[vec->size - 1];                                                          \
        if (vec->destroy_fn)                                                                       \
            vec->destroy_fn(&vec->data[--vec->size]);                                              \
        return res;                                                                                \
    }                                                                                              \
    void Name##_destroy(Name##_t *vec) {                                                           \
        for (size_t i = 0; i < vec->size; ++i) {                                                   \
            if (vec->destroy_fn)                                                                   \
                vec->destroy_fn(&vec->data[i]);                                                    \
        }                                                                                          \
        free(vec->data);                                                                           \
        vec->data = NULL;                                                                          \
        vec->size = 0;                                                                             \
        vec->capacity = 0;                                                                         \
    }                                                                                              \
    void Name##_reserve(Name##_t *vec, size_t capacity) {                                          \
        if (capacity > vec->capacity) {                                                            \
            __##Name##_realloc(vec, capacity);                                                     \
        }                                                                                          \
    }                                                                                              \
    void Name##_resize(Name##_t *vec, size_t size) {                                               \
        vec->size = size;                                                                          \
        if (size > vec->capacity) {                                                                \
            __##Name##_realloc(vec, size);                                                         \
        }                                                                                          \
    }                                                                                              \
    void Name##_clear(Name##_t *vec) {                                                             \
        for (size_t i = 0; i < vec->size; ++i) {                                                   \
            if (vec->destroy_fn)                                                                   \
                vec->destroy_fn(&vec->data[i]);                                                    \
        }                                                                                          \
        vec->size = 0;                                                                             \
    }                                                                                              \
    int Name##_empty(const Name##_t *vec) { return vec->size == 0; }

#define VECTOR_DECLARE_DEFINE(T, Name)                                                             \
    VECTOR_DECLARE(T, Name)                                                                        \
    VECTOR_DEFINE(T, Name)
