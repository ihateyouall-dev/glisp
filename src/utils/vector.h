#pragma once

#include <assert.h>
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
    const T *Name##_cat(Name *vec, size_t idx);                                                    \
    T Name##_pop_back(Name *vec);                                                                  \
    void Name##_destroy(Name *vec);

#define VECTOR_DEFINE(T, Name)                                                                     \
    void Name##_init(Name *vec) {                                                                  \
        vec->capacity = 8;                                                                         \
        vec->data = malloc(vec->capacity * sizeof(T));                                             \
        vec->size = 0;                                                                             \
    }                                                                                              \
    static int Name##_realloc(Name *vec, size_t capacity) {                                        \
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
            Name##_realloc(vec, vec->capacity * 2);                                                \
        }                                                                                          \
        vec->data[vec->size++] = val;                                                              \
    }                                                                                              \
    T *Name##_at(Name *vec, size_t idx) {                                                          \
        assert(idx < vec->size);                                                                   \
        return &vec->data[idx];                                                                    \
    }                                                                                              \
    const T *Name##_cat(Name *vec, size_t idx) {                                                   \
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
    }
