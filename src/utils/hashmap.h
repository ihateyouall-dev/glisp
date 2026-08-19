#pragma once

#include "vector.h"

#include <string.h>

#ifdef MSC_VER
#include "strdup.h"
#endif

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
    void Name##_destroy(Name##_t *map);

#define HASHMAP_DEFINE(T, Name)                                                                    \
    static void Name##Entry_destroy(Name##Entry_t *entry) {                                        \
        Name##_destroy_fn(&entry->value);                                                          \
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
        res.key = strdup(entry->key);                                                              \
        res.value = Name##_copy_fn(entry->value);                                                  \
        return res;                                                                                \
    }                                                                                              \
    static void __##Name##_rehash(Name##_t *map, size_t new_buckets_count) {                       \
        Name##Bucket_t *new = calloc(new_buckets_count, sizeof(Name##Bucket_t));                   \
        for (size_t i = 0; i < new_buckets_count; ++i) {                                           \
            Name##Bucket_init(&new[i], &Name##Entry_destroy);                                      \
        }                                                                                          \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket_t *bucket = &map->buckets[i];                                             \
            for (size_t j = 0; j < bucket->size; ++j) {                                            \
                Name##Entry_t *entry = Name##Bucket_at(bucket, j);                                 \
                Name##Entry_t copy = Name##Entry_copy(entry);                                      \
                assert(&copy != entry);                                                            \
                size_t idx = __##Name##_fnv1a(copy.key) % new_buckets_count;                       \
                Name##Bucket_push_back(&new[idx], copy);                                           \
            }                                                                                      \
        }                                                                                          \
        free(map->buckets);                                                                        \
        map->buckets = new;                                                                        \
        map->buckets_count = new_buckets_count;                                                    \
    }                                                                                              \
    void Name##_init(Name##_t *map, void (*destroy_fn)(T *), T (*copy_fn)(T)) {                    \
        map->buckets_count = 32;                                                                   \
        map->buckets = calloc(map->buckets_count, sizeof(Name##Bucket_t));                         \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket_init(&map->buckets[i], &Name##Entry_destroy);                             \
        }                                                                                          \
        map->size = 0;                                                                             \
        map->destroy_fn = destroy_fn;                                                              \
        Name##_destroy_fn = destroy_fn;                                                            \
        Name##_copy_fn = copy_fn;                                                                  \
    }                                                                                              \
    void Name##_insert(Name##_t *map, const char *key, T value) {                                  \
        Name##Bucket_t *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];        \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry_t *prev = Name##Bucket_at(bucket, i);                                      \
            if (strcmp(key, prev->key) == 0) {                                                     \
                map->destroy_fn(&prev->value);                                                     \
                prev->value = value;                                                               \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
        if (((double)(map->size + 1) / map->buckets_count) > 0.75) {                               \
            __##Name##_rehash(map, map->buckets_count * 2);                                        \
            bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];                    \
        }                                                                                          \
        Name##Entry_t entry;                                                                       \
        entry.key = strdup(key);                                                                   \
        entry.value = value;                                                                       \
        Name##Bucket_push_back(bucket, entry);                                                     \
        ++map->size;                                                                               \
    }                                                                                              \
    T *Name##_get(Name##_t *map, const char *key) {                                                \
        Name##Bucket_t *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];        \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry_t *entry = Name##Bucket_at(bucket, i);                                     \
            if (strcmp(key, entry->key) == 0) {                                                    \
                return &entry->value;                                                              \
            }                                                                                      \
        }                                                                                          \
        return NULL;                                                                               \
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
