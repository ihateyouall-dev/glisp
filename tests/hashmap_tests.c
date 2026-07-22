#include "hashmap.h"
#include "test.h"

int test_status = 0;

HASHMAP_DECLARE_DEFINE(int, map)

int main(void) {
    map_t hm;
    map_init(&hm);

    map_insert(&hm, "x", 12);
    TEST(*map_get(&hm, "x") == 12, "Access by key");
    TEST(hm.size == 1, "Size after inserting");

    map_insert(&hm, "y", 13);
    TEST(*map_get(&hm, "y") == 13, "Access by key");
    TEST(hm.size == 2, "Size after inserting");

    map_insert(&hm, "x", 42);
    TEST(*map_get(&hm, "x") == 42, "Rewriting by key");
    TEST(hm.size == 2, "Size after rewriting");

    TEST(map_get(&hm, "unknown") == NULL, "Unknown key");
    char keys[100][16];

    for (int i = 0; i < 100; ++i) {
        sprintf(keys[i], "key%d", i);
        map_insert(&hm, keys[i], i);
    }

    for (int i = 0; i < 100; ++i) {
        TEST(*map_get(&hm, keys[i]) == i, "Access after rehash");
    }

    map_destroy(&hm);

    TEST(hm.size == 0, "Size after destroy");
    TEST(hm.buckets == NULL, "Buckets after destroy");
    TEST(hm.buckets_count == 0, "Buckets count after destroy");

    return test_status;
}
