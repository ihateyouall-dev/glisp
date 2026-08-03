#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_DECLARE(T, Name)                                                                     \
    typedef struct {                                                                               \
        T *data;                                                                                   \
        size_t size;                                                                               \
        void (*destroy_fn)(T *);                                                                   \
    } Name##_t;                                                                                    \
    void Name##_init(Name##_t *arr, size_t size, void (*destroy_fn)(T *));                         \
    T *Name##_at(Name##_t *arr, size_t idx);                                                       \
    Name##_t *Name##_copy(Name##_t *arr);                                                          \
    void Name##_destroy(Name##_t *arr);

#define ARRAY_DEFINE(T, Name)                                                                      \
    void Name##_init(Name##_t *arr, size_t size, void (*destroy_fn)(T *)) {                        \
        arr->data = malloc(sizeof(T) * size);                                                      \
        arr->size = size;                                                                          \
        arr->destroy_fn = destroy_fn;                                                              \
    }                                                                                              \
    T *Name##_at(Name##_t *arr, size_t idx) {                                                      \
        assert(idx < arr->size);                                                                   \
        return &arr->data[idx];                                                                    \
    }                                                                                              \
    Name##_t *Name##_copy(Name##_t *arr) {                                                         \
        Name##_t *res = malloc(sizeof(Name##_t));                                                  \
        Name##_init(res, arr->size, arr->destroy_fn);                                              \
        memcpy(res->data, arr->data, sizeof(T) * res->size);                                       \
        return res;                                                                                \
    }                                                                                              \
    void Name##_destroy(Name##_t *arr) {                                                           \
        if (!arr || !arr->data)                                                                    \
            return;                                                                                \
        for (size_t i = 0; i < arr->size; ++i) {                                                   \
            arr->destroy_fn(arr->data + i);                                                        \
        }                                                                                          \
        free(arr->data);                                                                           \
        arr->size = 0;                                                                             \
        arr->data = NULL;                                                                          \
    }

#define ARRAY_DECLARE_DEFINE(T, Name)                                                              \
    ARRAY_DECLARE(T, Name)                                                                         \
    ARRAY_DEFINE(T, Name)
