#include "test.h"

#include "env.h"
#include "value.h"

int test_status = 0;

void test_value_copy(void) {
    gl_value_t *val = gl_value_make_int(42);
    gl_value_t *copy = gl_value_copy(val);

    TEST(val != copy, "Copy has another address");
    TEST(val->val != copy->val, "Copy value has another address");

    TEST(val->type == copy->type, "Copy has same value type");
    TEST((*(int64_t *)val->val) == (*(int64_t *)copy->val), "Copy has same value");
}

void test_derived_envs(void) {
    gl_env_t *global = gl_make_global_env();
    gl_env_t *derived = gl_make_env(gl_value_make_env(global));

    TEST(gl_env_get_var(global, "t"), "Global variable access");
    TEST(gl_env_get_fun(global, "defun"), "Global function access");
    TEST(gl_env_get_var(derived, "t"), "Derived variable access");
    TEST(gl_env_get_fun(derived, "defun"), "Derived function access");
    TEST(gl_env_get_var(global, "t")->val, "Global variable value access");
    TEST(gl_env_get_fun(global, "defun")->val, "Global function value access");
    TEST(gl_env_get_var(derived, "t")->val, "Derived variable value access");
    TEST(gl_env_get_fun(derived, "defun")->val, "Derived function value access");
}

int main(void) {
    test_derived_envs();
    test_value_copy();
    return test_status;
}
