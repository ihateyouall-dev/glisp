#include "env.h"
#include "memory.h"
#include "parser.h"
#include "runtime/eval.h"
#include "test.h"
#include "value.h"

int test_status = 0;

static gl_value_t *eval_str(const char *src) {
    gl_parser_t parser = gl_make_parser(src, "");
    gl_value_t *res = gl_parse_and_eval(&parser);
    gl_parser_destroy(&parser);
    return res;
}

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

    gl_env_destroy(derived);
}

void test_array_and_vector_eval(void) {
    gl_value_t *arr = eval_str("[1 (+ 2 3) 6]");
    TEST(arr != NULL, "Eval array not null");
    TEST(arr->type == GL_VAL_ARRAY, "Eval array type");
    gl_value_array_t *a = gl_value_get_array(arr);
    TEST(a->size == 3, "Array size");
    TEST(gl_value_get_int(a->data[0]) == 1, "Array elem 0");
    TEST(gl_value_get_int(a->data[1]) == 5, "Array elem 1");
    TEST(gl_value_get_int(a->data[2]) == 6, "Array elem 2");

    gl_value_t *vec = eval_str("{10 (* 2 3) 30}");
    TEST(vec != NULL, "Eval vector not null");
    TEST(vec->type == GL_VAL_VECTOR, "Eval vector type");
    gl_value_vector_t *v = gl_value_get_vector(vec);
    TEST(v->size == 3, "Vector size");
    TEST(gl_value_get_int(*gl_value_vector_at(v, 0)) == 10, "Vector elem 0");
    TEST(gl_value_get_int(*gl_value_vector_at(v, 1)) == 6, "Vector elem 1");
    TEST(gl_value_get_int(*gl_value_vector_at(v, 2)) == 30, "Vector elem 2");
}

void test_array_vector_predicates(void) {
    gl_value_t *r1 = eval_str("(array? [1 2 3])");
    TEST(gl_value_get_bool(r1) == 1, "array? on array");

    gl_value_t *r2 = eval_str("(array? {1 2 3})");
    TEST(gl_value_get_bool(r2) == 0, "array? on vector");

    gl_value_t *r3 = eval_str("(vector? {1 2 3})");
    TEST(gl_value_get_bool(r3) == 1, "vector? on vector");

    gl_value_t *r4 = eval_str("(vector? [1 2 3])");
    TEST(gl_value_get_bool(r4) == 0, "vector? on array");
}

void test_array_vector_constructors(void) {
    gl_value_t *arr = eval_str("(array 10 20 30)");
    TEST(arr->type == GL_VAL_ARRAY, "(array ...) creates array");
    gl_value_array_t *a = gl_value_get_array(arr);
    TEST(a->size == 3, "Array constructor size");
    TEST(gl_value_get_int(a->data[1]) == 20, "Array constructor elem 1");

    gl_value_t *vec = eval_str("(vector 100 200)");
    TEST(vec->type == GL_VAL_VECTOR, "(vector ...) creates vector");
    gl_value_vector_t *v = gl_value_get_vector(vec);
    TEST(v->size == 2, "Vector constructor size");
    TEST(gl_value_get_int(*gl_value_vector_at(v, 0)) == 100, "Vector constructor elem 0");
}

void test_array_vector_operations(void) {
    // len
    gl_value_t *l1 = eval_str("(len [1 2 3 4 5])");
    TEST(gl_value_get_int(l1) == 5, "len array");

    gl_value_t *l2 = eval_str("(len {1 2 3})");
    TEST(gl_value_get_int(l2) == 3, "len vector");

    // get
    gl_value_t *g1 = eval_str("(get [10 20 30] 1)");
    TEST(gl_value_get_int(g1) == 20, "get array");

    gl_value_t *g2 = eval_str("(get {10 20 30} 2)");
    TEST(gl_value_get_int(g2) == 30, "get vector");

    // set-at on array
    eval_str("(set a [1 2 3])");
    eval_str("(set-at a 1 99)");
    gl_value_t *elem_a = eval_str("(get a 1)");
    TEST(gl_value_get_int(elem_a) == 99, "set-at array");

    // set-at on vector
    eval_str("(set v {10 20 30})");
    eval_str("(set-at v 0 42)");
    gl_value_t *elem_v = eval_str("(get v 0)");
    TEST(gl_value_get_int(elem_v) == 42, "set-at vector");

    // push on vector
    eval_str("(push v 40 50)");
    gl_value_t *v_len = eval_str("(len v)");
    TEST(gl_value_get_int(v_len) == 5, "push increased vector len");
    gl_value_t *v_last = eval_str("(get v 4)");
    TEST(gl_value_get_int(v_last) == 50, "push added elements");

    // pop on vector
    gl_value_t *popped = eval_str("(pop v)");
    TEST(gl_value_get_int(popped) == 50, "pop returns popped element");
    gl_value_t *v_len_after_pop = eval_str("(len v)");
    TEST(gl_value_get_int(v_len_after_pop) == 4, "pop decreased vector len");
}

void test_array_vector_comparison(void) {
    gl_value_t *eq1 = eval_str("(= [1 2 3] [1 2 3])");
    TEST(gl_value_get_bool(eq1) == 1, "Array equality");

    gl_value_t *eq2 = eval_str("(= [1 2 3] [1 2 4])");
    TEST(gl_value_get_bool(eq2) == 0, "Array inequality");

    gl_value_t *eq3 = eval_str("(= {1 2 3} {1 2 3})");
    TEST(gl_value_get_bool(eq3) == 1, "Vector equality");

    gl_value_t *eq4 = eval_str("(= {1 2 3} {1 2 4})");
    TEST(gl_value_get_bool(eq4) == 0, "Vector inequality");

    gl_value_t *eq5 = eval_str("(= [1 2] {1 2})");
    TEST(gl_value_get_bool(eq5) == 0, "Array and vector not equal");
}

void test_array_vector_gc(void) {
    eval_str("(progn (set gcv {1 2 3}) (set gca [4 5 6]) (gc-collect))");
    gl_value_t *v = eval_str("gcv");
    TEST(v->type == GL_VAL_VECTOR, "Vector survives GC");
    TEST(gl_value_get_vector(v)->size == 3, "Vector size after GC");

    gl_value_t *a = eval_str("gca");
    TEST(a->type == GL_VAL_ARRAY, "Array survives GC");
    TEST(gl_value_get_array(a)->size == 3, "Array size after GC");
}

void test_typeid(void) {
    TEST(gl_value_get_bool(eval_str("(= (typeof 42) int-t)")) == 1, "typeof int");
    TEST(gl_value_get_bool(eval_str("(= (typeof 3.14) float-t)")) == 1, "typeof float");
    TEST(gl_value_get_bool(eval_str("(= (typeof \"abc\") string-t)")) == 1, "typeof string");
    TEST(gl_value_get_bool(eval_str("(= (typeof 'sym) symbol-t)")) == 1, "typeof symbol");
    TEST(gl_value_get_bool(eval_str("(= (typeof nil) nil-t)")) == 1, "typeof nil");
    TEST(gl_value_get_bool(eval_str("(= (typeof (list 1 2)) list-t)")) == 1, "typeof list");
    TEST(gl_value_get_bool(eval_str("(= (typeof [1 2]) array-t)")) == 1, "typeof array");
    TEST(gl_value_get_bool(eval_str("(= (typeof {1 2}) vector-t)")) == 1, "typeof vector");
    TEST(gl_value_get_bool(eval_str("(= (typeof (function print)) function-t)")) == 1,
         "typeof function");
    TEST(gl_value_get_bool(eval_str("(= (typeof (lambda (x) x)) function-t)")) == 1,
         "typeof lambda");
    TEST(gl_value_get_bool(eval_str("(= (typeof (typeof 1)) typeid-t)")) == 1, "typeof typeid");

    TEST(gl_value_get_bool(eval_str("(typeid? int-t)")) == 1, "typeid? true");
    TEST(gl_value_get_bool(eval_str("(typeid? (typeof 42))")) == 1, "typeid? on typeof result");
    TEST(gl_value_get_bool(eval_str("(typeid? 42)")) == 0, "typeid? false on int");

    TEST(gl_value_get_bool(eval_str("(= (typeof 42) float-t)")) == 0, "int-t != float-t");
    TEST(gl_value_get_bool(eval_str("(= (typeof 42) nil)")) == 0, "typeid != nil");
    TEST(gl_value_get_bool(eval_str("(= (typeof 42) \"int\")")) == 0, "typeid != string");
    TEST(gl_value_get_bool(eval_str("(= (typeof 42) 42)")) == 0, "typeid != number");
}

void test_let(void) {
    TEST(gl_value_get_int(eval_str("(let ((x 10) (y 20)) (+ x y))")) == 30, "Basic let");

    eval_str("(set x 1)");
    TEST(gl_value_get_int(eval_str("(let ((x 2) (y x)) y)")) == 1, "Parallel let evaluation");

    TEST(gl_value_get_int(eval_str("(let ((a 100)) (let ((b 200) (a 300)) (+ a b)))")) == 500,
         "Nested let shadowing");

    TEST(eval_str("(let (z) z)")->type == GL_VAL_NIL, "Bare symbol in let binds to nil");

    TEST(gl_value_get_int(eval_str("(let () 42)")) == 42, "Empty let bindings");

    TEST(eval_str("(let ((x 1)))")->type == GL_VAL_NIL, "Empty let body returns nil");

    TEST(gl_value_get_int(eval_str(
             "(let ((count 0)) (set count (+ count 5)) (set count (+ count 10)) count)")) == 15,
         "Mutation with set in let");

    eval_str("(defun make-adder (n) (let ((base n)) (lambda (x) (+ base x))))");
    eval_str("(set add5 (make-adder 5))");
    TEST(gl_value_get_int(eval_str("(add5 10)")) == 15, "Closure over let variable");
}

void test_let_star(void) {
    TEST(gl_value_get_int(eval_str("(let* ((x 10) (y 20)) (+ x y))")) == 30, "Basic let*");

    eval_str("(set x 1)");
    TEST(gl_value_get_int(eval_str("(let* ((x 2) (y x)) y)")) == 2, "Sequential let* evaluation");

    TEST(gl_value_get_int(eval_str("(let* ((a 5) (a (+ a 10))) a)")) == 15,
         "let* sequential shadowing");

    TEST(gl_value_get_int(
             eval_str("(let* ((a 1) (b (+ a 1)) (c (+ b 1)) (d (+ c 1))) (+ a b c d))")) == 10,
         "let* chained bindings");
}

int main(void) {
    test_derived_envs();
    test_value_copy();

    gl_init_global_env();
    test_array_and_vector_eval();
    test_array_vector_predicates();
    test_array_vector_constructors();
    test_array_vector_operations();
    test_array_vector_comparison();
    test_array_vector_gc();
    test_typeid();
    test_let();
    test_let_star();

    gl_destroy_memory_heap();
    return test_status;
}
