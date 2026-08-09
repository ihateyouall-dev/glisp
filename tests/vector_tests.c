#include "test.h"
#include "vector.h"

int test_status = 0;

VECTOR_DECLARE_DEFINE(int, vector)

void dummy(int *) {} // NOLINT
int copy_fn(int x) { return x; }

int main(void) {
    vector_t vec;
    vector_init(&vec, dummy);

    TEST(vec.size == 0, "Initial size");
    TEST(vec.capacity == 8, "Initial capacity");

    vector_push_back(&vec, 1);

    TEST(vec.size == 1, "Size after push");
    TEST(*vector_at(&vec, 0) == 1, "Access");

    // This will cause reallocation and increase capacity
    for (int i = 2; i <= 9; ++i) {
        vector_push_back(&vec, i);
    }

    TEST(vec.capacity == 16, "Capacity after reallocation");

    TEST(*vector_at(&vec, 8) == 9, "Access after reallocation");

    TEST(vector_pop_back(&vec) == 9, "Pop value");
    TEST(vec.size == 8, "Size after pop");

    for (int i = 0; i < 8; ++i) {
        TEST(*vector_at(&vec, i) == i + 1, "Data preserved");
    }

    vector_t copy = vector_copy(&vec, copy_fn);

    TEST(copy.size == vec.size, "Size of copy");
    TEST(copy.capacity == vec.capacity, "Capacity of copy");

    for (int i = 0; i < 8; ++i) {
        TEST(*vector_at(&copy, i) == *vector_at(&vec, i), "Copy data preserved");
    }

    vector_reserve(&vec, 100);

    TEST(vec.capacity == 100, "Reservation");

    vector_resize(&vec, 120);

    TEST(vec.size == 120, "Size after resizing");
    TEST(vec.capacity == 120, "Capacity after resizing");

    vector_destroy(&vec);

    TEST(vec.data == NULL, "Data after destroy");
    TEST(vec.size == 0, "Size after destroy");
    TEST(vec.capacity == 0, "Capacity after destroy");

    return test_status;
}
