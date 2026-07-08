#pragma once

#include "vector.h"

#include <string.h>

#define HASHMAP_DECLARE(T, Name)                                                                   \
    typedef struct {                                                                               \
        const char *key;                                                                           \
        T value;                                                                                   \
    } Name##Entry;                                                                                 \
    VECTOR_DECLARE(Name##Entry, Name##Bucket)                                                      \
    typedef struct {                                                                               \
        Name##Bucket *buckets;                                                                     \
        size_t size;                                                                               \
        size_t buckets_count;                                                                      \
    } Name;                                                                                        \
    void Name##_init(Name *map);                                                                   \
    void Name##_insert(Name *map, const char *key, T value);                                       \
    T *Name##_get(Name *map, const char *key);                                                     \
    void Name##_destroy(Name *map);

#define HASHMAP_DEFINE(T, Name)                                                                    \
    VECTOR_DEFINE(Name##Entry, Name##Bucket)                                                       \
    static unsigned long long __##Name##_fnv1a(const char *key) {                                  \
        unsigned long long res = 14695981039346656037ULL;                                          \
        const unsigned long long prime = 1099511628211ULL;                                         \
        while (*key) {                                                                             \
            res ^= (unsigned char)*key++;                                                          \
            res *= prime;                                                                          \
        }                                                                                          \
        return res;                                                                                \
    }                                                                                              \
    static void __##Name##_rehash(Name *map, size_t new_buckets_count) {                           \
        Name##Bucket *new = calloc(new_buckets_count, sizeof(Name##Bucket));                       \
        for (size_t i = 0; i < new_buckets_count; ++i) {                                           \
            Name##Bucket_init(&new[i]);                                                            \
        }                                                                                          \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket *bucket = &map->buckets[i];                                               \
            for (size_t j = 0; j < bucket->size; ++j) {                                            \
                Name##Entry *entry = Name##Bucket_at(bucket, j);                                   \
                size_t idx = __##Name##_fnv1a(entry->key) % new_buckets_count;                     \
                Name##Bucket_push_back(&new[idx], *entry);                                         \
            }                                                                                      \
        }                                                                                          \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket_destroy(&map->buckets[i]);                                                \
        }                                                                                          \
        free(map->buckets);                                                                        \
        map->buckets = new;                                                                        \
        map->buckets_count = new_buckets_count;                                                    \
    }                                                                                              \
    void Name##_init(Name *map) {                                                                  \
        map->buckets_count = 32;                                                                   \
        map->buckets = calloc(map->buckets_count, sizeof(Name##Bucket));                           \
        for (size_t i = 0; i < map->buckets_count; ++i) {                                          \
            Name##Bucket_init(&map->buckets[i]);                                                   \
        }                                                                                          \
        map->size = 0;                                                                             \
    }                                                                                              \
    void Name##_insert(Name *map, const char *key, T value) {                                      \
        Name##Bucket *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];          \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry *prev = Name##Bucket_at(bucket, i);                                        \
            if (strcmp(key, prev->key) == 0) {                                                     \
                prev->value = value;                                                               \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
        if (((double)(map->size + 1) / map->buckets_count) > 0.75) {                               \
            __##Name##_rehash(map, map->buckets_count * 2);                                        \
            bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];                    \
        }                                                                                          \
        Name##Entry entry;                                                                         \
        entry.key = key;                                                                           \
        entry.value = value;                                                                       \
        Name##Bucket_push_back(bucket, entry);                                                     \
        ++map->size;                                                                               \
    }                                                                                              \
    T *Name##_get(Name *map, const char *key) {                                                    \
        Name##Bucket *bucket = &map->buckets[__##Name##_fnv1a(key) % map->buckets_count];          \
        for (size_t i = 0; i < bucket->size; ++i) {                                                \
            Name##Entry *entry = Name##Bucket_at(bucket, i);                                       \
            if (strcmp(key, entry->key) == 0) {                                                    \
                return &entry->value;                                                              \
            }                                                                                      \
        }                                                                                          \
        return NULL;                                                                               \
    }                                                                                              \
    void Name##_destroy(Name *map) {                                                               \
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
