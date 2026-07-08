#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define VECTOR_DECLARE(T, Name)                                                                    \
    typedef struct {                                                                               \
        T *data;                                                                                   \
        size_t size;                                                                               \
        size_t capacity;                                                                           \
    } Name;                                                                                        \
    void Name##_init(Name *vec);                                                                   \
    void Name##_push_back(Name *vec, T val);                                                       \
    T *Name##_at(Name *vec, size_t idx);                                                           \
    T Name##_pop_back(Name *vec);                                                                  \
    void Name##_destroy(Name *vec);                                                                \
    void Name##_reserve(Name *vec, size_t capacity);                                               \
    void Name##_resize(Name *vec, size_t size);                                                    \
    void Name##_clear(Name *vec);                                                                  \
    int Name##_empty(const Name *vec);

#define VECTOR_DEFINE(T, Name)                                                                     \
    void Name##_init(Name *vec) {                                                                  \
        vec->capacity = 8;                                                                         \
        vec->data = malloc(vec->capacity * sizeof(T));                                             \
        vec->size = 0;                                                                             \
    }                                                                                              \
    static int __##Name##_realloc(Name *vec, size_t capacity) {                                    \
        T *tmp = realloc(vec->data, capacity * sizeof(T));                                         \
        if (tmp) {                                                                                 \
            vec->data = tmp;                                                                       \
            vec->capacity = capacity;                                                              \
            return 1;                                                                              \
        }                                                                                          \
        return 0;                                                                                  \
    }                                                                                              \
    void Name##_push_back(Name *vec, T val) {                                                      \
        if (vec->size == vec->capacity) {                                                          \
            __##Name##_realloc(vec, vec->capacity * 2);                                            \
        }                                                                                          \
        vec->data[vec->size++] = val;                                                              \
    }                                                                                              \
    T *Name##_at(Name *vec, size_t idx) {                                                          \
        assert(idx < vec->size);                                                                   \
        return &vec->data[idx];                                                                    \
    }                                                                                              \
    T Name##_pop_back(Name *vec) {                                                                 \
        assert(vec->size > 0);                                                                     \
        return vec->data[--vec->size];                                                             \
    }                                                                                              \
    void Name##_destroy(Name *vec) {                                                               \
        free(vec->data);                                                                           \
        vec->data = NULL;                                                                          \
        vec->size = 0;                                                                             \
        vec->capacity = 0;                                                                         \
    }                                                                                              \
    void Name##_reserve(Name *vec, size_t capacity) {                                              \
        if (capacity > vec->capacity) {                                                            \
            __##Name##_realloc(vec, capacity);                                                     \
        }                                                                                          \
    }                                                                                              \
    void Name##_resize(Name *vec, size_t size) {                                                   \
        vec->size = size;                                                                          \
        if (size > vec->capacity) {                                                                \
            __##Name##_realloc(vec, size);                                                         \
        }                                                                                          \
    }                                                                                              \
    void Name##_clear(Name *vec) { vec->size = 0; }                                                \
    int Name##_empty(const Name *vec) { return vec->size == 0; }

#define VECTOR_DECLARE_DEFINE(T, Name)                                                             \
    VECTOR_DECLARE(T, Name)                                                                        \
    VECTOR_DEFINE(T, Name)
