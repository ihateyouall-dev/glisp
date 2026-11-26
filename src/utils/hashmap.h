#pragma once

/**
 * @file hashmap.h
 * @brief Generic string-keyed hash map macros.
 */

#include "alloc.h"
#include "vector.h"

#include <string.h>

/**
 * Declares a string-keyed hash map named @p Name with values of type @p T.
 *
 * Use HASHMAP_DEFINE(T, Name) in exactly one translation unit. Keys are copied,
 * buckets chain on collisions, and the load factor is kept below 0.75 by
 * doubling the bucket count. Element ownership is controlled by the
 * destroy callback passed to Name_init.
 */
#define HASHMAP_DECLARE(T, Name)                                                                   \
    static void (*Name##_destroy_fn)(T *) = NULL;                                                  \
    static T (*Name##_copy_fn)(T) = NULL;                                                          \
    typedef struct {                                                                               \
        char *key;                                                                                 \
        T value;                                                                                   \
    } Name##Entry_t;                                                                               \
    VECTOR_DECLARE(Name##Entry_t, Name##Bucket)                                                    \
    typedef struct {                                                                               \
        Name##Bucket_t *buckets;                                                                   \
        size_t size;                                                                               \
        size_t buckets_count;                                                                      \
        void (*destroy_fn)(T *);                                                                   \
    } Name##_t;                                                                                    \
    void Name##_init(Name##_t *map, void (*destroy_fn)(T *), T (*copy_fn)(T));                     \
    void Name##_insert(Name##_t *map, const char *key, T value);                                   \
    T *Name##_get(Name##_t *map, const char *key);                                                 \
    int Name##_remove(Name##_t *map, const char *key);                                             \
    void Name##_destroy(Name##_t *map);

#define HASHMAP_DEFINE(T, Name)                                                                    \
    static void Name##Entry_destroy(Name##Entry_t *entry) {                                        \
        if (Name##_destroy_fn) {                                                                   \
            Name##_destroy_fn(&entry->value);                                                      \
        }                                                                                          \
        free(entry->key);                                                                          \
    }                                                                                              \
    VECTOR_DEFINE(Name##Entry_t, Name##Bucket)                                                     \
    static unsigned long long __##Name##_fnv1a(const char *key) {                                  \
        unsigned long long res = 14695981039346656037ULL;                                          \
        const unsigned long long prime = 1099511628211ULL;                                         \
        while (*key) {                                                                             \
            res ^= (unsigned char)*key++;                                                          \
            res *= prime;                                                                          \
        }                                                                                          \
        return res;                                                                                \
    }                                                                                              \
    static Name##Entry_t Name##Entry_copy(Name##Entry_t *entry) {                                  \
        Name##Entry_t res;                                                                         \
        res.key = gl_xstrdup(entry->key);                                                          \
        res.value = Name##_copy_fn ? Name##_copy_fn(entry->value) : entry->value;                  \
        return res;                                                                                \
    }                                                                                              \
    static void __##Name##_init_buckets(Name##_t *map, size_t count) {                             \
        map->buckets = gl_xcalloc(count, sizeof(Name##Bucket_t));                                  \
        map->buckets_count = count;                                                                \
        for (size_t i = 0; i < count; ++i) {                                                       \
            Name##Bucket_init(&map->buckets[i], &Name##Entry_destroy);                             \
        }                                                                                          \
    }                                                                                              \
    static void __##Name##_rehash(Name##_t *map, size_t new_buckets_count) {                       \
        Name##Bucket_t *old = map->buckets;                                                        \
        size_t old_count = map->buckets_count;                                                     \
        __##Name##_init_buckets(map, new_buckets_count);                                           \
        for (size_t i = 0; i < old_count; ++i) {                                                   \
            Name##Bucket_t *bucket = &old[i];                                                      \
            for (size_t j = 0; j < bucket->size; ++j) {                                            \
                Name##Entry_t *entry = Name##Bucket_at(bucket, j);                                 \
                size_t idx = __##Name##_fnv1a(entry->key) % new_buckets_count;                     \
                Name##Bucket_push_back(&map->buckets[idx], *entry);                                \
            }                                                                                      \
            free(bucket->data);                                                                    \
        }                                                                                          \
        free(old);                                                                                 \
    }                                                                                              \
    void Name##_init(Name##_t *map, void (*destroy_fn)(T *), T (*copy_fn)(T)) {                    \
        map->buckets_count = 0;                                                                    \
        map->buckets = NULL;                                                                       \
        map->size = 0;                                                                             \
        map->destroy_fn = destroy_fn;                                                              \
        Name##_destroy_fn = destroy_fn;                                                            \
        Name##_copy_fn = copy_fn;                                                                  \
    }                                                                                              \
    void Name##_insert(Name##_t *map, const char *key, T value) {                                  \
        if (map->buckets_count == 0) {                                                             \
            __##Name##_init_buckets(map, 4);                                                       \
        }                                                                                          \
        Name##Bucket_t *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];        \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry_t *prev = Name##Bucket_at(bucket, i);                                      \
            if (strcmp(key, prev->key) == 0) {                                                     \
                if (map->destroy_fn) {                                                             \
                    map->destroy_fn(&prev->value);                                                 \
                }                                                                                  \
                prev->value = value;                                                               \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
        if (((double)(map->size + 1) / map->buckets_count) > 0.75) {                               \
            __##Name##_rehash(map, map->buckets_count * 2);                                        \
            bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];                    \
        }                                                                                          \
        Name##Entry_t entry;                                                                       \
        entry.key = gl_xstrdup(key);                                                               \
        entry.value = value;                                                                       \
        Name##Bucket_push_back(bucket, entry);                                                     \
        ++map->size;                                                                               \
    }                                                                                              \
    T *Name##_get(Name##_t *map, const char *key) {                                                \
        if (map->buckets_count == 0) {                                                             \
            return NULL;                                                                           \
        }                                                                                          \
        Name##Bucket_t *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];        \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry_t *entry = Name##Bucket_at(bucket, i);                                     \
            if (strcmp(key, entry->key) == 0) {                                                    \
                return &entry->value;                                                              \
            }                                                                                      \
        }                                                                                          \
        return NULL;                                                                               \
    }                                                                                              \
    int Name##_remove(Name##_t *map, const char *key) {                                            \
        if (map->buckets_count == 0) {                                                             \
            return 0;                                                                              \
        }                                                                                          \
        Name##Bucket_t *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];        \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry_t *entry = Name##Bucket_at(bucket, i);                                     \
            if (strcmp(key, entry->key) == 0) {                                                    \
                free(entry->key);                                                                  \
                for (size_t j = i + 1; j < bucket->size; ++j) {                                    \
                    bucket->data[j - 1] = bucket->data[j];                                         \
                }                                                                                  \
                --bucket->size;                                                                    \
                --map->size;                                                                       \
                return 1;                                                                          \
            }                                                                                      \
        }                                                                                          \
        return 0;                                                                                  \
    }                                                                                              \
    void Name##_destroy(Name##_t *map) {                                                           \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket_destroy(&map->buckets[i]);                                                \
        }                                                                                          \
        map->size = 0;                                                                             \
        free(map->buckets);                                                                        \
        map->buckets = NULL;                                                                       \
        map->buckets_count = 0;                                                                    \
    }

#define HASHMAP_DECLARE_DEFINE(T, Name)                                                            \
    HASHMAP_DECLARE(T, Name)                                                                       \
    HASHMAP_DEFINE(T, Name)
