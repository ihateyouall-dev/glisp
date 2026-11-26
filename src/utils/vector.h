#pragma once

/**
 * @file vector.h
 * @brief Generic growable vector macros.
 */

#include "alloc.h"

#include <assert.h>
#include <stddef.h>

/**
 * Declares a growable typed vector named @p Name with elements of type @p T.
 *
 * Use VECTOR_DEFINE(T, Name) in exactly one translation unit. Capacity grows
 * geometrically on push. Elements are stored by value; an optional destroy
 * callback is invoked for each element on clear/destroy.
 */
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
    int Name##_empty(const Name##_t *vec);                                                         \
    Name##_t Name##_copy(Name##_t *vec, T (*copy_fn)(T));

#define VECTOR_DEFINE(T, Name)                                                                     \
    void Name##_init(Name##_t *vec, void (*destroy_fn)(T *)) {                                     \
        vec->capacity = 0;                                                                         \
        vec->data = NULL;                                                                          \
        vec->size = 0;                                                                             \
        vec->destroy_fn = destroy_fn;                                                              \
    }                                                                                              \
    static void __##Name##_grow(Name##_t *vec, size_t capacity) {                                  \
        vec->data = gl_xrealloc(vec->data, capacity * sizeof(T));                                  \
        vec->capacity = capacity;                                                                  \
    }                                                                                              \
    void Name##_push_back(Name##_t *vec, T val) {                                                  \
        if (vec->size == vec->capacity) {                                                          \
            __##Name##_grow(vec, vec->capacity ? vec->capacity * 2 : 8);                           \
        }                                                                                          \
        vec->data[vec->size++] = val;                                                              \
    }                                                                                              \
    T *Name##_at(Name##_t *vec, size_t idx) {                                                      \
        assert(idx < vec->size);                                                                   \
        return &vec->data[idx];                                                                    \
    }                                                                                              \
    T Name##_pop_back(Name##_t *vec) {                                                             \
        assert(vec->size > 0);                                                                     \
        vec->size--;                                                                               \
        return vec->data[vec->size];                                                               \
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
            __##Name##_grow(vec, capacity);                                                        \
        }                                                                                          \
    }                                                                                              \
    void Name##_resize(Name##_t *vec, size_t size) {                                               \
        if (size > vec->capacity) {                                                                \
            __##Name##_grow(vec, size);                                                            \
        }                                                                                          \
        vec->size = size;                                                                          \
    }                                                                                              \
    void Name##_clear(Name##_t *vec) {                                                             \
        for (size_t i = 0; i < vec->size; ++i) {                                                   \
            if (vec->destroy_fn)                                                                   \
                vec->destroy_fn(&vec->data[i]);                                                    \
        }                                                                                          \
        vec->size = 0;                                                                             \
    }                                                                                              \
    int Name##_empty(const Name##_t *vec) { return vec->size == 0; }                               \
    Name##_t Name##_copy(Name##_t *vec, T (*copy_fn)(T)) {                                         \
        Name##_t res;                                                                              \
        Name##_init(&res, vec->destroy_fn);                                                        \
        Name##_reserve(&res, vec->size);                                                           \
        Name##_reserve(&res, vec->capacity);                                                       \
        res.size = vec->size;                                                                      \
        for (size_t i = 0; i < vec->size; ++i) {                                                   \
            res.data[i] = copy_fn ? copy_fn(vec->data[i]) : vec->data[i];                          \
        }                                                                                          \
        return res;                                                                                \
    }

#define VECTOR_DECLARE_DEFINE(T, Name)                                                             \
    VECTOR_DECLARE(T, Name)                                                                        \
    VECTOR_DEFINE(T, Name)
