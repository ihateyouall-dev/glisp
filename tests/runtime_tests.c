#include "builtins.h"
#include "callstack.h"
#include "env.h"
#include "memory.h"
#include "parser.h"
#include "runtime/eval.h"
#include "test.h"
#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static int gl_test_setenv(const char *name, const char *value) { return _putenv_s(name, value); }

static void gl_test_unsetenv(const char *name) { _putenv_s(name, ""); }
#else
static int gl_test_setenv(const char *name, const char *value) { return setenv(name, value, 1); }

static void gl_test_unsetenv(const char *name) { unsetenv(name); }
#endif

static int gl_test_chdir(const char *path) {
#ifdef _WIN32
    return _chdir(path);
#else
    return chdir(path);
#endif
}

static int gl_test_make_temp(char *path, size_t size) {
#ifdef _WIN32
    const char *dir = getenv("TEMP");
    if (dir == NULL) {
        dir = ".";
    }
    snprintf(path, size, "%s\\glisp_io_test_%ld.txt", dir, (long)_getpid());
    // GLisp strings interpret backslashes as C escapes, so the path embedded
    // into source below has to use forward slashes.
    for (char *p = path; *p != '\0'; ++p) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }
    fclose(f);
    return 0;
#else
    snprintf(path, size, "/tmp/glisp_io_test_XXXXXX");
    int fd = mkstemp(path);
    if (fd == -1) {
        return -1;
    }
    close(fd);
    return 0;
#endif
}

int test_status = 0;

static gl_value_t *eval_str(const char *src) {
    gl_parser_t parser = gl_make_parser(src, "");
    gl_value_t *res = gl_parse_and_eval(&parser);
    gl_parser_destroy(&parser);
    return res;
}

static gl_value_t *eval_expr(const char *src) {
    gl_parser_t parser = gl_make_parser(src, "");
    gl_ast_node_t *node = gl_parser_parse(&parser);
    gl_value_t *res = NULL;
    if (node) {
        res = gl_eval(node);
        gl_ast_destroy(node);
    }
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

void test_singletons(void) {
    gl_value_t *nil1 = gl_value_make_nil();
    gl_value_t *nil2 = gl_value_make_nil();
    TEST(nil1 == nil2, "nil is a singleton");

    gl_value_t *t1 = gl_value_make_bool(1);
    gl_value_t *t2 = gl_value_make_bool(1);
    TEST(t1 == t2, "t is a singleton");

    gl_value_t *nil_copy = gl_value_copy(nil1);
    TEST(nil_copy == nil1, "copying nil returns the singleton");

    gl_value_t *t_copy = gl_value_copy(t1);
    TEST(t_copy == t1, "copying t returns the singleton");
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
    if (arr == NULL) {
        return;
    }
    TEST(arr->type == GL_VAL_ARRAY, "Eval array type");
    gl_value_array_t *a = gl_value_get_array(arr);
    TEST(a->size == 3, "Array size");
    TEST(gl_value_get_int(a->data[0]) == 1, "Array elem 0");
    TEST(gl_value_get_int(a->data[1]) == 5, "Array elem 1");
    TEST(gl_value_get_int(a->data[2]) == 6, "Array elem 2");

    gl_value_t *vec = eval_str("{10 (* 2 3) 30}");
    TEST(vec != NULL, "Eval vector not null");
    if (vec == NULL) {
        return;
    }
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
    eval_str("(progn (set gcv {1 2 3}) (set gca [4 5 6]) (pragma gc-collect))");
    gl_value_t *v = eval_str("gcv");
    TEST(v->type == GL_VAL_VECTOR, "Vector survives GC");
    TEST(gl_value_get_vector(v)->size == 3, "Vector size after GC");

    gl_value_t *a = eval_str("gca");
    TEST(a->type == GL_VAL_ARRAY, "Array survives GC");
    TEST(gl_value_get_array(a)->size == 3, "Array size after GC");
}

void test_hashmap(void) {
    gl_value_t *empty = eval_str("(hash)");
    TEST(empty->type == GL_VAL_HASHMAP, "hash creates a hashmap");
    TEST(gl_value_get_hashmap(empty)->size == 0, "New hashmap is empty");
    TEST(gl_value_get_int(eval_str("(len (hash))")) == 0, "Empty hashmap len");

    eval_str("(set hm (hash))");
    eval_str("(hash-set hm \"name\" \"glisp\")");
    TEST(strcmp(gl_value_get_string(eval_str("(hash-get hm \"name\")")), "glisp") == 0,
         "get returns stored value");
    TEST(gl_value_get_int(eval_str("(len hm)")) == 1, "len after set");

    TEST(eval_str("(hash-get hm \"missing\")")->type == GL_VAL_NIL, "get missing key is nil");
    TEST(gl_value_get_int(eval_str("(hash-get hm \"missing\" 7)")) == 7, "get default value");

    TEST(gl_value_get_bool(eval_str("(hash-has? hm \"name\")")) == 1, "has? on present key");
    TEST(gl_value_get_bool(eval_str("(hash-has? hm \"missing\")")) == 0, "has? on missing key");

    eval_str("(hash-set hm \"name\" \"other\")");
    TEST(strcmp(gl_value_get_string(eval_str("(hash-get hm \"name\")")), "other") == 0,
         "set replaces existing key");
    TEST(gl_value_get_int(eval_str("(len hm)")) == 1, "len after replacement");

    TEST(gl_value_get_bool(eval_str("(hash-delete hm \"name\")")) == 1, "delete returns t");
    TEST(gl_value_get_bool(eval_str("(hash-delete hm \"name\")")) == 0, "delete of missing key");
    TEST(gl_value_get_int(eval_str("(len hm)")) == 0, "len after delete");

    TEST(gl_value_get_int(eval_str("(let ((m (hash))) (hash-set m \"a\" 1) (hash-set m \"b\" 2) "
                                   "(len (hash-keys m)))")) == 2,
         "keys length");

    TEST(gl_value_get_int(eval_str("(let ((m (hash))) (hash-set m \"a\" 1) "
                                   "(hash-get m \"a\"))")) == 1,
         "hashmap in let binding");

    eval_str("(set gc-hm (hash))");
    eval_str("(hash-set gc-hm \"k\" (list 1 2 3))");
    eval_str("(pragma gc-collect)");
    gl_value_t *gc_hm = eval_str("gc-hm");
    TEST(gc_hm->type == GL_VAL_HASHMAP, "Hashmap survives GC");
    TEST(gl_value_get_int(eval_str("(len gc-hm)")) == 1, "Hashmap len after GC");
    TEST(gl_value_get_int(eval_str("(car (hash-get gc-hm \"k\"))")) == 1,
         "Hashmap contents after GC");
}

void test_name_suggestions(void) {
    gl_env_t *global = (gl_env_t *)gl_global_env->val;

    char *suggestion = gl_env_suggest_name(global, "lenght");
    TEST(suggestion != NULL && strcmp(suggestion, "length") == 0, "suggests length for lenght");
    free(suggestion);

    suggestion = gl_env_suggest_name(global, "printn");
    TEST(suggestion != NULL && strcmp(suggestion, "print") == 0, "suggests print for printn");
    free(suggestion);

    suggestion = gl_env_suggest_name(global, "cont");
    TEST(suggestion != NULL && strcmp(suggestion, "cond") == 0, "suggests cond for cont");
    free(suggestion);

    suggestion = gl_env_suggest_name(global, "zzz");
    TEST(suggestion == NULL, "no suggestion for a name that is far from everything");
    free(suggestion);

    suggestion = gl_env_suggest_name(global, "qqqqq");
    TEST(suggestion == NULL, "no suggestion for a name with nothing close");
    free(suggestion);

    // The search crosses frames: a local binding is suggested from an inner one.
    gl_env_t *inner = gl_make_env(gl_global_env);
    gl_env_set_var(inner, "local-counter", gl_value_make_int(1));
    suggestion = gl_env_suggest_name(inner, "local-counterr");
    TEST(suggestion != NULL && strcmp(suggestion, "local-counter") == 0,
         "suggests a binding from an enclosing frame");
    free(suggestion);
    gl_env_destroy(inner);
}

void test_std_integer_math(void) {
    TEST(gl_value_get_int(eval_str("(mod 10 3)")) == 1, "mod of positive numbers");
    TEST(gl_value_get_int(eval_str("(mod -10 3)")) == 2, "mod follows the divisor sign");
    TEST(gl_value_get_int(eval_str("(mod 10 -3)")) == -2, "mod with a negative divisor");
    TEST(gl_value_get_int(eval_str("(mod -10 -3)")) == -1, "mod of two negatives");
    TEST(gl_value_get_bool(eval_str("(int? (mod 10 3))")) == 1, "mod of ints stays an int");

    TEST(gl_value_get_int(eval_str("(rem 10 3)")) == 1, "rem of positive numbers");
    TEST(gl_value_get_int(eval_str("(rem -10 3)")) == -1, "rem follows the dividend sign");
    TEST(gl_value_get_int(eval_str("(rem 10 -3)")) == 1, "rem with a negative divisor");
    TEST(gl_value_get_bool(eval_str("(int? (rem 10 3))")) == 1, "rem of ints stays an int");

    TEST(gl_value_get_bool(eval_str("(approx= (mod 5.5 2) 1.5)")) == 1, "mod of floats");
    TEST(gl_value_get_bool(eval_str("(approx= (rem -5.5 2) -1.5)")) == 1, "rem of floats");
}

static int __gl_has_text(gl_value_t *val, const char *expected) {
    char *text = gl_value_to_string(val);
    int same = text != NULL && strcmp(text, expected) == 0;
    free(text);
    return same;
}

void test_arithmetic_builtins(void) {
    TEST(gl_value_get_int(eval_str("(% 10 3)")) == 1, "modulo of positive numbers");
    TEST(gl_value_get_int(eval_str("(% -10 3)")) == -1, "modulo keeps the dividend sign");
    TEST(gl_value_get_int(eval_str("(int 2.9)")) == 2, "int truncates toward zero");
    TEST(gl_value_get_int(eval_str("(int -2.9)")) == -2, "int truncates negatives toward zero");
    TEST(gl_value_get_int(eval_str("(int 7)")) == 7, "int of an int is unchanged");
    TEST(gl_value_get_bool(eval_str("(approx= (float 2) 2.0)")) == 1, "float widens an int");
    TEST(gl_value_get_int(eval_str("(+ 1 2 3 4)")) == 10, "addition of several arguments");
    TEST(gl_value_get_int(eval_str("(- 10 3 2)")) == 5, "left associative subtraction");
    TEST(gl_value_get_int(eval_str("(/ 7 2)")) == 3, "integer division truncates");
    TEST(eval_expr("(/ 1 0)") == NULL, "division by integer zero is an error");
    TEST(eval_expr("(% 1 0)") == NULL, "modulo by zero is an error");
    TEST(__gl_has_text(eval_expr("(+ 9223372036854775807 1)"), "9223372036854775808"),
         "addition overflow promotes to a bignum");
    TEST(__gl_has_text(eval_expr("(- 0 9223372036854775807 2)"), "-9223372036854775809"),
         "subtraction overflow promotes to a bignum");
    TEST(__gl_has_text(eval_expr("(* 9223372036854775807 2)"), "18446744073709551614"),
         "multiplication overflow promotes to a bignum");
    TEST(__gl_has_text(eval_expr("(- (- 0 9223372036854775807 1))"), "9223372036854775808"),
         "negation overflow promotes to a bignum");
    TEST(__gl_has_text(eval_expr("(/ (- 0 9223372036854775807 1) -1)"), "9223372036854775808"),
         "division overflow promotes to a bignum");
    TEST(gl_value_get_int(eval_expr("(% (- 0 9223372036854775807 1) -1)")) == 0,
         "remainder of the minimum int by -1 is zero");
    TEST(eval_expr("(int (* 1.0 10000000000.0 1000000000000.0))") == NULL,
         "int conversion overflow is an error");
    TEST(eval_expr("(int (/ 0.0 0.0))") == NULL, "int conversion of NaN is an error");
    TEST(gl_value_get_int(eval_str("(+ 9223372036854775806 1)")) == INT64_MAX,
         "addition up to the limit stays exact");
}

void test_bignum_arithmetic(void) {
    TEST(__gl_has_text(eval_str("(* 99999999999999999999 99999999999999999999)"),
                       "9999999999999999999800000000000000000001"),
         "big multiplication is exact");
    TEST(__gl_has_text(eval_str("(/ 100000000000000000000 3)"), "33333333333333333333"),
         "big division is exact");
    TEST(gl_value_get_int(eval_str("(% 100000000000000000000 7)")) == 2, "big remainder is exact");
    TEST(gl_value_get_int(eval_str("(% -100000000000000000000 7)")) == -2,
         "big remainder keeps the dividend sign");
    TEST(__gl_has_text(eval_str("(- 0 9223372036854775808)"), "-9223372036854775808"),
         "a bignum result that fits is reduced to int");
    TEST(gl_value_get_int(eval_str("(- 9223372036854775808 9223372036854775807)")) == 1,
         "mixed bignum and int subtraction");
    TEST(gl_value_get_bool(eval_str("(= 4294967296 0)")) == 0,
         "equality does not truncate integers to 32 bits");
    TEST(gl_value_get_bool(eval_str("(< 4294967296 4294967297)")) == 1,
         "comparison does not truncate integers to 32 bits");
    TEST(gl_value_get_bool(eval_str("(> 9223372036854775808 9223372036854775807)")) == 1,
         "bignum compares above the int64 limit");
    TEST(gl_value_get_bool(eval_str("(< -9223372036854775809 -9223372036854775808)")) == 1,
         "negative bignum compares below the int64 limit");
    TEST(gl_value_get_bool(
             eval_str("(approx= (float 9223372036854775808) 9223372036854775808.0)")) == 1,
         "float conversion accepts bignums");
    TEST(eval_expr("(int 9223372036854775808)") == NULL,
         "int conversion rejects an out-of-range bignum");
    TEST(__gl_has_text(
             eval_str("(factorial 100)"),
             "933262154439441526816992388562667004907159682643816214685929638952175999932299"
             "15608941463976156518286253697920827223758251185210916864000000000000000000000000"),
         "factorial of 100 is exact");
    TEST(__gl_has_text(eval_str("(typeof (factorial 100))"), "bignum"),
         "a large factorial has the bignum type");
    TEST(gl_value_get_bool(eval_str("(int? (factorial 100))")) == 1,
         "bignums satisfy the integer predicate");
    TEST(gl_value_get_bool(eval_str("(bignum? 1)")) == 0, "small integers are not bignums");
    TEST(gl_value_get_int(eval_str("(mod (factorial 50) 97)")) == 65,
         "floored mod keeps bignum precision");
    TEST(gl_value_get_int(eval_str("(rem (factorial 50) 97)")) == 65,
         "remainder keeps bignum precision");
    TEST(gl_value_get_int(eval_str("(gcd (factorial 100) (- (factorial 100) 1))")) == 1,
         "gcd handles bignums");
    TEST(__gl_has_text(eval_str("(string->number \"9223372036854775808\")"), "9223372036854775808"),
         "string->number produces bignums");
    TEST(__gl_has_text(eval_str("(string->number \"-9223372036854775809\")"),
                       "-9223372036854775809"),
         "string->number accepts negative bignums");
}

void test_comparison_builtins(void) {
    TEST(gl_value_get_bool(eval_str("(> 3 2)")) == 1, "greater than");
    TEST(gl_value_get_bool(eval_str("(> 2 3)")) == 0, "greater than is false");
    TEST(gl_value_get_bool(eval_str("(>= 3 3)")) == 1, "greater or equal");
    TEST(gl_value_get_bool(eval_str("(<= 2 3)")) == 1, "less or equal");
    TEST(gl_value_get_bool(eval_str("(<= 3 2)")) == 0, "less or equal is false");
    TEST(gl_value_get_bool(eval_str("(< 1 1.5)")) == 1, "mixed int and float compare");
    TEST(gl_value_get_bool(eval_str("(< \"abc\" \"abd\")")) == 1, "strings compare by content");
    TEST(gl_value_get_bool(eval_str("(= 'a 'a)")) == 1, "symbols compare by name");
    TEST(gl_value_get_bool(eval_str("(= 1 1.0)")) == 1, "int and float compare numerically");
}

void test_sequence_aliases(void) {
    TEST(gl_value_get_int(eval_str("(length (list 1 2 3))")) == 3, "length alias");
    TEST(gl_value_get_int(eval_str("(nth (list 10 20) 1)")) == 20, "nth alias");
    TEST(gl_value_get_int(eval_str("(aref [10 20] 1)")) == 20, "aref alias");
    TEST(gl_value_get_int(eval_str("(aset [1 2] 0 9)")) == 9, "aset returns the new value");
    TEST(gl_value_get_int(eval_str("(let ((v [1 2])) (aset v 0 9) (get v 0))")) == 9,
         "aset mutates in place");
    TEST(gl_value_get_int(eval_str("(len (vector-push {1} 2 3))")) == 3, "vector-push alias");
    TEST(gl_value_get_int(eval_str("(vector-pop {1 2})")) == 2, "vector-pop alias");
}

void test_math_builtins(void) {
    TEST(gl_value_get_bool(eval_str("(approx= (sqrt 9) 3.0)")) == 1, "sqrt");
    TEST(gl_value_get_bool(eval_str("(approx= (pow 2 10) 1024.0)")) == 1, "pow");
    TEST(gl_value_get_bool(eval_str("(approx= (exp 0) 1.0)")) == 1, "exp of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (exp2 10) 1024.0)")) == 1, "exp2");
    TEST(gl_value_get_bool(eval_str("(approx= (log 1) 0.0)")) == 1, "log of one");
    TEST(gl_value_get_bool(eval_str("(approx= (log2 8) 3.0)")) == 1, "log2");
    TEST(gl_value_get_bool(eval_str("(approx= (floor 2.7) 2.0)")) == 1, "floor");
    TEST(gl_value_get_bool(eval_str("(approx= (floor -2.5) -3.0)")) == 1, "floor of a negative");
    TEST(gl_value_get_bool(eval_str("(approx= (ceil 2.1) 3.0)")) == 1, "ceil");
    TEST(gl_value_get_bool(eval_str("(approx= (ceil -2.5) -2.0)")) == 1, "ceil of a negative");
    TEST(gl_value_get_bool(eval_str("(approx= (round 2.5) 3.0)")) == 1,
         "round half away from zero");
    TEST(gl_value_get_bool(eval_str("(approx= (round -2.5) -3.0)")) == 1, "round a negative half");
    TEST(gl_value_get_bool(eval_str("(approx= (sin 0) 0.0)")) == 1, "sin of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (cos 0) 1.0)")) == 1, "cos of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (atan2 1 1) (/ PI 4))")) == 1, "atan2");
    TEST(gl_value_get_bool(eval_str("(approx= (fma 2 3 4) 10.0)")) == 1, "fma");
    TEST(gl_value_get_bool(eval_str("(approx= (copysign 3 -1) -3.0)")) == 1, "copysign");
    TEST(gl_value_get_bool(eval_str("(approx= (copysign -3 1) 3.0)")) == 1,
         "copysign of a negative");
}

void test_bitwise_builtins(void) {
    TEST(gl_value_get_int(eval_str("(logand 12 10)")) == 8, "logand");
    TEST(gl_value_get_int(eval_str("(logior 12 10)")) == 14, "logior");
    TEST(gl_value_get_int(eval_str("(logxor 12 10)")) == 6, "logxor");
    TEST(gl_value_get_int(eval_str("(lognot 0)")) == -1, "lognot of zero");
    TEST(gl_value_get_bool(eval_str("(= (lognot (lognot 5)) 5)")) == 1, "double lognot");
}

void test_std_iteration(void) {
    eval_str("(set each-sum 0)");
    eval_str("(each (lambda (x) (set each-sum (+ each-sum x))) (list 1 2 3 4))");
    TEST(gl_value_get_int(eval_str("each-sum")) == 10, "each visits every element");
    TEST(eval_str("(each (lambda (x) x) (list 1))")->type == GL_VAL_NIL, "each returns nil");

    eval_str("(set each-vec 0)");
    eval_str("(each (lambda (x) (set each-vec (+ each-vec x))) {1 2 3})");
    TEST(gl_value_get_int(eval_str("each-vec")) == 6, "each over a vector");

    eval_str("(set each-str 0)");
    eval_str("(each (lambda (c) (set each-str (+ each-str 1))) \"abc\")");
    TEST(gl_value_get_int(eval_str("each-str")) == 3, "each over a string");

    TEST(gl_value_get_int(eval_str("(reduce (lambda (a b) (+ a b)) [1 2 3] 10)")) == 16,
         "reduce over an array with an initial value");
    TEST(gl_value_get_int(eval_str("(len (map 1+ (range 0 4)))")) == 4,
         "map over a generated list");
}

void test_std_math_functions(void) {
    TEST(gl_value_get_int(eval_str("(factorial 5)")) == 120, "factorial");
    TEST(gl_value_get_int(eval_str("(factorial 0)")) == 1, "factorial of zero");
    TEST(gl_value_get_int(eval_str("(gcd 12 18)")) == 6, "gcd");
    TEST(gl_value_get_int(eval_str("(lcm 4 6)")) == 12, "lcm");
    TEST(gl_value_get_int(eval_str("(lcm 0 5)")) == 0, "lcm with zero");
    TEST(gl_value_get_bool(eval_str("(approx= (hypot 3 4) 5.0)")) == 1, "hypot");
    TEST(gl_value_get_bool(eval_str("(approx= (lerp 0 10 0.5) 5.0)")) == 1, "lerp");
    TEST(gl_value_get_bool(eval_str("(approx= (trunc 2.9) 2.0)")) == 1, "trunc");
    TEST(gl_value_get_bool(eval_str("(approx= (trunc -2.9) -2.0)")) == 1, "trunc of a negative");
    TEST(gl_value_get_int(eval_str("(signum -5)")) == -1, "signum of a negative");
    TEST(gl_value_get_int(eval_str("(signum 0)")) == 0, "signum of zero");
    TEST(gl_value_get_int(eval_str("(signum 5)")) == 1, "signum of a positive");
    TEST(gl_value_get_bool(eval_str("(approx= (deg->rad 180) PI)")) == 1, "degrees to radians");
    TEST(gl_value_get_bool(eval_str("(approx= (rad->deg PI) 180.0)")) == 1, "radians to degrees");
    TEST(gl_value_get_bool(eval_str("(approx= (cbrt 27) 3.0)")) == 1, "cube root");
    TEST(gl_value_get_bool(eval_str("(approx= (cbrt -8) -2.0)")) == 1, "cube root of a negative");

    TEST(gl_value_get_bool(eval_str("(approx= (tan 0) 0.0)")) == 1, "tan");
    TEST(gl_value_get_bool(eval_str("(approx= (cot (/ PI 4)) 1.0)")) == 1, "cot");
    TEST(gl_value_get_bool(eval_str("(approx= (sec 0) 1.0)")) == 1, "sec");
    TEST(gl_value_get_bool(eval_str("(approx= (csc (/ PI 2)) 1.0)")) == 1, "csc");
    TEST(gl_value_get_bool(eval_str("(approx= (asin 1) (/ PI 2))")) == 1, "asin");
    TEST(gl_value_get_bool(eval_str("(approx= (acos 1) 0.0)")) == 1, "acos of one");
    TEST(gl_value_get_bool(eval_str("(approx= (atan 1) (/ PI 4))")) == 1, "atan");
    TEST(gl_value_get_bool(eval_str("(approx= (acot 1) (/ PI 4))")) == 1, "acot");
    TEST(gl_value_get_bool(eval_str("(approx= (asec 1) 0.0)")) == 1, "asec of one");
    TEST(gl_value_get_bool(eval_str("(approx= (acsc 1) (/ PI 2))")) == 1, "acsc of one");

    TEST(gl_value_get_bool(eval_str("(approx= (sinh 0) 0.0)")) == 1, "sinh of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (cosh 0) 1.0)")) == 1, "cosh of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (tanh 0) 0.0)")) == 1, "tanh of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (coth 1) (/ 1.0 (tanh 1)))")) == 1, "coth");
    TEST(gl_value_get_bool(eval_str("(approx= (sech 0) 1.0)")) == 1, "sech of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (csch 1) (/ 1.0 (sinh 1)))")) == 1, "csch");
    TEST(gl_value_get_bool(eval_str("(approx= (asinh 0) 0.0)")) == 1, "asinh of zero");
    TEST(gl_value_get_bool(eval_str("(approx= (acosh 1) 0.0)")) == 1, "acosh of one");
    TEST(gl_value_get_bool(eval_str("(approx= (atanh 0.5) (* 0.5 (log 3)))")) == 1, "atanh");
    TEST(gl_value_get_bool(eval_str("(approx= (acoth 2) (atanh (/ 1.0 2)))")) == 1, "acoth");
    TEST(gl_value_get_bool(eval_str("(approx= (asech 1) 0.0)")) == 1, "asech of one");
    TEST(gl_value_get_bool(eval_str("(approx= (acsch 1) (asinh 1))")) == 1, "acsch");

    TEST(gl_value_get_bool(eval_str("(frac? 2.5)")) == 1, "frac? of a fractional value");
    TEST(gl_value_get_bool(eval_str("(frac? 2.0)")) == 0, "frac? of a whole float");
    TEST(gl_value_get_bool(eval_str("(frac? 2)")) == 0, "frac? of an int");
    TEST(gl_value_get_bool(eval_str("(natural? 0)")) == 1, "natural? of zero");
    TEST(gl_value_get_bool(eval_str("(natural? -1)")) == 0, "natural? of a negative");
    TEST(gl_value_get_bool(eval_str("(natural? 2.5)")) == 0, "natural? of a fraction");

    TEST(gl_value_get_int(eval_str("(min 3 5)")) == 3, "min");
    TEST(gl_value_get_int(eval_str("(max 3 5)")) == 5, "max");
    TEST(gl_value_get_int(eval_str("(clamp 10 0 5)")) == 5, "clamp above the range");
    TEST(gl_value_get_int(eval_str("(clamp -1 0 5)")) == 0, "clamp below the range");
    TEST(gl_value_get_int(eval_str("(clamp 3 0 5)")) == 3, "clamp inside the range");
    TEST(gl_value_get_int(eval_str("(1+ 41)")) == 42, "1+");
    TEST(gl_value_get_int(eval_str("(1- 43)")) == 42, "1-");
    TEST(gl_value_get_bool(eval_str("(even? 4)")) == 1, "even?");
    TEST(gl_value_get_bool(eval_str("(odd? 4)")) == 0, "odd? of an even number");
    TEST(gl_value_get_bool(eval_str("(approx= (abs -7) 7)")) == 1, "abs of a negative");
}

void test_std_predicates(void) {
    TEST(gl_value_get_bool(eval_str("(instanceof? int-t 5)")) == 1, "instanceof? match");
    TEST(gl_value_get_bool(eval_str("(instanceof? string-t 5)")) == 0, "instanceof? mismatch");
    TEST(gl_value_get_bool(eval_str("(int? 5)")) == 1, "int?");
    TEST(gl_value_get_bool(eval_str("(int? 5.0)")) == 0, "int? of a float");
    TEST(gl_value_get_bool(eval_str("(float? 5.0)")) == 1, "float?");
    TEST(gl_value_get_bool(eval_str("(number? 5.0)")) == 1, "number? of a float");
    TEST(gl_value_get_bool(eval_str("(string? \"x\")")) == 1, "string?");
    TEST(gl_value_get_bool(eval_str("(symbol? 'x)")) == 1, "symbol?");
    TEST(gl_value_get_bool(eval_str("(nil? nil)")) == 1, "nil?");
    TEST(gl_value_get_bool(eval_str("(function? car)")) == 1, "function? of a builtin");
    TEST(gl_value_get_bool(eval_str("(function? (lambda (x) x))")) == 1, "function? of a lambda");
    TEST(gl_value_get_bool(eval_str("(list? (list 1))")) == 1, "list?");
    TEST(gl_value_get_bool(eval_str("(array? [1])")) == 1, "array?");
    TEST(gl_value_get_bool(eval_str("(vector? {1})")) == 1, "vector?");
    TEST(gl_value_get_bool(eval_str("(hash? (hash))")) == 1, "hash?");
    TEST(gl_value_get_bool(eval_str("(collection? {1})")) == 1, "collection?");
    TEST(gl_value_get_bool(eval_str("(collection? 1)")) == 0, "collection? of an int");
    TEST(gl_value_get_bool(eval_str("(file? *stdout*)")) == 1, "file?");
    TEST(gl_value_get_bool(eval_str("(type= 1 2)")) == 1, "type= of same-typed values");
    TEST(gl_value_get_bool(eval_str("(type= 1 \"a\")")) == 0, "type= of different types");
    TEST(gl_value_get_bool(eval_str("(type= int-t float-t)")) == 1,
         "type= compares the type of its arguments, so two typeids match");
    TEST(gl_value_get_bool(eval_str("(null? nil)")) == 1, "null? alias");
}

void test_std_list_accessors(void) {
    TEST(gl_value_get_int(eval_str("(last (list 1 2 3))")) == 3, "last");
    TEST(gl_value_get_int(eval_str("(caar (list (list 1 2) 3))")) == 1, "caar");
    TEST(gl_value_get_int(eval_str("(cadr (list 1 2 3))")) == 2, "cadr");
    TEST(gl_value_get_int(eval_str("(car (cdar (list (list 1 2))))")) == 2, "cdar");
    TEST(gl_value_get_int(eval_str("(car (cddr (list 1 2 3)))")) == 3, "cddr");
    TEST(gl_value_get_int(eval_str("(caddr (list 1 2 3))")) == 3, "caddr");
    TEST(gl_value_get_int(eval_str("(identity 42)")) == 42, "identity");
    TEST(gl_value_get_int(eval_str("(id 42)")) == 42, "id alias");
}

void test_std_io_helpers(void) {
    char path[512];
    if (gl_test_make_temp(path, sizeof(path)) != 0) {
        TEST(0, "create temporary io file");
        return;
    }

    char src[1024];
    snprintf(src, sizeof(src), "(set io-helper (open \"%s\" \"w\"))", path);
    TEST(eval_str(src) != NULL, "open a file for the io helpers");

    TEST(gl_value_get_int(eval_str("(write 7 io-helper)")) == 1, "write returns the byte count");
    TEST(eval_str("(write-each (list 8 9) io-helper)")->type == GL_VAL_NIL,
         "write-each returns nil");
    TEST(eval_str("(fprint io-helper \"x\")") != NULL, "fprint writes");
    TEST(eval_str("(fprintln io-helper \"y\")") != NULL, "fprintln writes a line");
    TEST(eval_str("(feprintln io-helper \"z\")")->type == GL_VAL_NIL, "feprintln writes a line");
    TEST(gl_value_get_bool(eval_str("(closed? io-helper)")) == 0, "closed? on an open file");
    eval_str("(close io-helper)");
    TEST(gl_value_get_bool(eval_str("(closed? io-helper)")) == 1, "closed? on a closed file");

    snprintf(src, sizeof(src), "(set io-helper (open \"%s\" \"r\"))", path);
    eval_str(src);
    gl_value_t *line = eval_str("(read-line io-helper)");
    TEST(line != NULL && line->type == GL_VAL_STRING &&
             strcmp(gl_value_get_string(line), "789xy") == 0,
         "the io helpers wrote the expected first line");
    line = eval_str("(read-line io-helper)");
    TEST(line != NULL && line->type == GL_VAL_STRING && strcmp(gl_value_get_string(line), "z") == 0,
         "feprintln starts a new line");
    eval_str("(close io-helper)");
    remove(path);
}

void test_logic_forms(void) {
    TEST(gl_value_get_bool(eval_str("(and)")) == 1, "and without operands is true");
    TEST(eval_str("(or)")->type == GL_VAL_NIL, "or without operands is nil");
    TEST(gl_value_get_bool(eval_str("(and 1 2)")) == 1, "and of true operands");
    TEST(gl_value_get_bool(eval_str("(and 1 nil)")) == 0, "and stops at nil");
    TEST(gl_value_get_bool(eval_str("(or nil 3)")) == 1, "or of a true operand");
    TEST(eval_str("(or nil nil)")->type == GL_VAL_NIL, "or of only nil");

    // Both forms short-circuit, so the undefined call is never reached.
    TEST(eval_str("(and nil (undefined-thing))")->type == GL_VAL_NIL, "and short-circuits");
    TEST(gl_value_get_bool(eval_str("(or t (undefined-thing))")) == 1, "or short-circuits");

    // A failure inside a reached operand has to propagate instead of crashing.
    TEST(eval_expr("(and (undefined-thing) t)") == NULL, "and reports a failing operand");
    TEST(eval_expr("(or (undefined-thing) t)") == NULL, "or reports a failing operand");
}

void test_copy(void) {
    eval_str("(set copy-source {1 2})");
    eval_str("(set copy-alias copy-source)");
    eval_str("(push copy-alias 3)");
    TEST(gl_value_get_int(eval_str("(len copy-source)")) == 3, "assignment aliases a vector");

    eval_str("(set copy-source {1 2})");
    eval_str("(set copy-made (copy copy-source))");
    eval_str("(push copy-made 3)");
    TEST(gl_value_get_int(eval_str("(len copy-source)")) == 2, "copy keeps the source unchanged");
    TEST(gl_value_get_int(eval_str("(len copy-made)")) == 3, "copy is independently mutable");

    eval_str("(set copy-nested (list {1 2}))");
    eval_str("(set copy-nested2 (copy copy-nested))");
    eval_str("(push (car copy-nested2) 3)");
    TEST(gl_value_get_int(eval_str("(len (car copy-nested))")) == 2, "copy is deep for lists");
    TEST(gl_value_get_int(eval_str("(len (car copy-nested2))")) == 3, "nested copy is mutable");

    eval_str("(set copy-map (hash))");
    eval_str("(hash-set copy-map \"k\" {1})");
    eval_str("(set copy-map2 (copy copy-map))");
    eval_str("(push (hash-get copy-map2 \"k\") 2)");
    TEST(gl_value_get_int(eval_str("(len (hash-get copy-map \"k\"))")) == 1,
         "copy is deep for hash maps");

    TEST(gl_value_get_int(eval_str("(copy 5)")) == 5, "copy of an int");
    TEST(strcmp(gl_value_get_string(eval_str("(copy \"abc\")")), "abc") == 0, "copy of a string");
    TEST(eval_str("(copy nil)")->type == GL_VAL_NIL, "copy of nil");
}

void test_float_printing(void) {
    TEST(strcmp(gl_value_get_string(eval_str("(to-string 1.5)")), "1.5") == 0,
         "float prints short");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string 3.14)")), "3.14") == 0,
         "float keeps its digits");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string 1.0)")), "1.0") == 0,
         "integral float keeps the point");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string -0.5)")), "-0.5") == 0,
         "negative float prints with its sign");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string (+ 0.1 0.2))")), "0.3") == 0,
         "float does not show binary noise");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string 1000000.0)")), "1000000.0") == 0,
         "large integral float stays in fixed form");
    TEST(strcmp(gl_value_get_string(eval_str("(to-string (/ 1.0 3.0))")), "0.333333333333333") == 0,
         "float keeps fifteen significant digits");
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

void test_strings(void) {
    TEST(strcmp(gl_value_get_string(eval_str("(string-append \"foo\" \"bar\" \"baz\")")),
                "foobarbaz") == 0,
         "string-append concatenates every argument");
    TEST(strcmp(gl_value_get_string(eval_str("(string-append)")), "") == 0,
         "string-append with no arguments");
    TEST(strcmp(gl_value_get_string(eval_str("(string-append \"a\" (to-string 12))")), "a12") == 0,
         "string-append with to-string");

    TEST(strcmp(gl_value_get_string(eval_str("(substring \"abcdef\" 2 4)")), "cd") == 0,
         "substring with end");
    TEST(strcmp(gl_value_get_string(eval_str("(substring \"abcdef\" 3)")), "def") == 0,
         "substring defaults to end of string");
    TEST(strcmp(gl_value_get_string(eval_str("(substring \"abcdef\" 0 0)")), "") == 0,
         "substring of empty range");

    TEST(strcmp(gl_value_get_string(eval_str("(string-upcase \"aBc1\")")), "ABC1") == 0,
         "string-upcase");
    TEST(strcmp(gl_value_get_string(eval_str("(string-downcase \"aBc1\")")), "abc1") == 0,
         "string-downcase");

    TEST(gl_value_get_int(eval_str("(string->number \"42\")")) == 42, "string->number int");
    TEST(gl_value_get_int(eval_str("(string->number \"-7\")")) == -7, "string->number negative");
    TEST(gl_value_get_bool(eval_str("(= (typeof (string->number \"2.5\")) float-t)")) == 1,
         "string->number float type");
    TEST(gl_value_get_bool(eval_str("(approx= (string->number \"2.5\") 2.5)")) == 1,
         "string->number float value");
    TEST(eval_str("(string->number \"\")")->type == GL_VAL_NIL, "string->number empty");
    TEST(eval_str("(string->number \"abc\")")->type == GL_VAL_NIL, "string->number text");
    TEST(eval_str("(string->number \"12abc\")")->type == GL_VAL_NIL,
         "string->number trailing text");
    TEST(eval_str("(string->number \"1e9\")")->type == GL_VAL_NIL, "string->number exponent");
    TEST(eval_str("(string->number \"1.\")")->type == GL_VAL_NIL, "string->number bare dot");
    TEST(__gl_has_text(eval_str("(string->number \"99999999999999999999999\")"),
                       "99999999999999999999999"),
         "string->number large integer becomes a bignum");
}

void test_std_strings(void) {
    TEST(gl_value_get_int(eval_str("(string-index \"hello\" \"ll\")")) == 2, "string-index match");
    TEST(gl_value_get_int(eval_str("(string-index \"hello\" \"h\")")) == 0,
         "string-index at start");
    TEST(gl_value_get_int(eval_str("(string-index \"hello\" \"\")")) == 0,
         "string-index empty needle");
    TEST(eval_str("(string-index \"hello\" \"z\")")->type == GL_VAL_NIL,
         "string-index absent needle");
    TEST(eval_str("(string-index \"he\" \"hello\")")->type == GL_VAL_NIL,
         "string-index needle longer than haystack");

    TEST(gl_value_get_bool(eval_str("(string-contains? \"hello\" \"ell\")")) == 1,
         "string-contains? match");
    TEST(gl_value_get_bool(eval_str("(string-contains? \"hello\" \"z\")")) == 0,
         "string-contains? absent needle");

    TEST(gl_value_get_bool(eval_str("(= (string-split \"a,b,c\" \",\") "
                                    "(list \"a\" \"b\" \"c\"))")) == 1,
         "string-split single character separator");
    TEST(gl_value_get_bool(eval_str("(= (string-split \"a::b::c\" \"::\") "
                                    "(list \"a\" \"b\" \"c\"))")) == 1,
         "string-split multi-character separator");
    TEST(gl_value_get_bool(eval_str("(= (string-split \"a,\" \",\") (list \"a\" \"\"))")) == 1,
         "string-split trailing separator");
    TEST(gl_value_get_bool(eval_str("(= (string-split \"abc\" \",\") (list \"abc\"))")) == 1,
         "string-split without separator");
    TEST(gl_value_get_bool(eval_str("(= (string-split \"\" \",\") (list \"\"))")) == 1,
         "string-split empty string");

    TEST(strcmp(gl_value_get_string(eval_str("(string-join (list \"a\" \"b\") \"-\")")), "a-b") ==
             0,
         "string-join with separator");
    TEST(strcmp(gl_value_get_string(eval_str("(string-join (list 1 2 3) \"-\")")), "1-2-3") == 0,
         "string-join converts with to-string");
    TEST(strcmp(gl_value_get_string(eval_str("(string-join (list \"a\") \"-\")")), "a") == 0,
         "string-join single element");
    TEST(strcmp(gl_value_get_string(eval_str("(string-join nil \"-\")")), "") == 0,
         "string-join empty list");
    TEST(strcmp(
             gl_value_get_string(eval_str("(string-join (string-split \"a::b::c\" \"::\") \"-\")")),
             "a-b-c") == 0,
         "string-join of string-split result");

    TEST(eval_str("(string-split \"a,b\" \"\")") == NULL, "string-split empty separator");

    TEST(gl_value_get_bool(eval_str("(string-starts-with? \"hello\" \"he\")")) == 1,
         "string-starts-with? match");
    TEST(gl_value_get_bool(eval_str("(string-starts-with? \"hello\" \"lo\")")) == 0,
         "string-starts-with? mismatch");
    TEST(gl_value_get_bool(eval_str("(string-starts-with? \"he\" \"hello\")")) == 0,
         "string-starts-with? longer prefix");
    TEST(gl_value_get_bool(eval_str("(string-ends-with? \"hello\" \"lo\")")) == 1,
         "string-ends-with? match");
    TEST(gl_value_get_bool(eval_str("(string-ends-with? \"hello\" \"\")")) == 1,
         "string-ends-with? empty suffix");
    TEST(gl_value_get_bool(eval_str("(string-ends-with? \"hello\" \"he\")")) == 0,
         "string-ends-with? mismatch");

    TEST(strcmp(gl_value_get_string(eval_str("(string-repeat \"ab\" 3)")), "ababab") == 0,
         "string-repeat");
    TEST(strcmp(gl_value_get_string(eval_str("(string-repeat \"ab\" 0)")), "") == 0,
         "string-repeat zero");

    TEST(strcmp(gl_value_get_string(eval_str("(string-replace \"a-b-c\" \"-\" \"+\")")), "a+b+c") ==
             0,
         "string-replace every occurrence");
    TEST(strcmp(gl_value_get_string(eval_str("(string-replace \"aaa\" \"aa\" \"b\")")), "ba") == 0,
         "string-replace left to right");
    TEST(strcmp(gl_value_get_string(eval_str("(string-replace \"abc\" \"z\" \"y\")")), "abc") == 0,
         "string-replace without a match");

    TEST(strcmp(gl_value_get_string(eval_str("(string-trim \"  hi  \")")), "hi") == 0,
         "string-trim removes both ends");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \" \")")) == 1,
         "string-whitespace? space");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"\\t\")")) == 1,
         "string-whitespace? tab");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"\\n\")")) == 1,
         "string-whitespace? newline");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"\\r\")")) == 1,
         "string-whitespace? carriage return");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"x\")")) == 0,
         "string-whitespace? letter");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"  \")")) == 0,
         "string-whitespace? rejects two characters");
    TEST(gl_value_get_bool(eval_str("(string-whitespace? \"\")")) == 0,
         "string-whitespace? rejects the empty string");
    TEST(strcmp(gl_value_get_string(eval_str("(string-trim-left \"  hi  \")")), "hi  ") == 0,
         "string-trim-left");
    TEST(strcmp(gl_value_get_string(eval_str("(string-trim-right \"  hi  \")")), "  hi") == 0,
         "string-trim-right");
    TEST(gl_value_get_int(eval_str("(len (string-trim \"   \"))")) == 0, "string-trim blank");

    TEST(strcmp(gl_value_get_string(eval_str("(format \"{} has {}\" \"cart\" 3)")), "cart has 3") ==
             0,
         "format substitutes placeholders");
    TEST(strcmp(gl_value_get_string(eval_str("(format \"plain\")")), "plain") == 0,
         "format without placeholders");
    TEST(strcmp(gl_value_get_string(eval_str("(format \"{}{}\" 1 2)")), "12") == 0,
         "format adjacent placeholders");
    TEST(eval_expr("(format \"{}\")") == NULL, "format with too few arguments");

    TEST(strcmp(gl_value_get_string(eval_str("(string-pad-left \"7\" 3)")), "  7") == 0,
         "string-pad-left");
    TEST(strcmp(gl_value_get_string(eval_str("(string-pad-right \"7\" 3)")), "7  ") == 0,
         "string-pad-right");
    TEST(strcmp(gl_value_get_string(eval_str("(string-pad-left \"abcd\" 2)")), "abcd") == 0,
         "string-pad-left keeps longer strings");

    TEST(gl_value_get_int(eval_str("(len (string-lines \"a\\nb\\nc\"))")) == 3,
         "string-lines count");
    TEST(strcmp(gl_value_get_string(eval_str("(car (string-lines \"a\\nb\"))")), "a") == 0,
         "string-lines first line");
    TEST(gl_value_get_int(eval_str("(len (string-lines \"a\\nb\\n\"))")) == 3 - 1,
         "string-lines drops one trailing empty line");
    TEST(gl_value_get_int(eval_str("(len (string-lines \"a\\r\\nb\\r\\n\"))")) == 2,
         "string-lines accepts carriage returns");
    TEST(eval_str("(string-lines \"\")")->type == GL_VAL_NIL, "string-lines of an empty string");
    TEST(gl_value_get_int(eval_str("(len (string-lines \"single\"))")) == 1,
         "string-lines of one line");
}

void test_range(void) {
    TEST(gl_value_get_int(eval_str("(len (range 0 5))")) == 5, "range length");
    TEST(gl_value_get_int(eval_str("(car (range 0 5))")) == 0, "range starts at the first bound");
    TEST(gl_value_get_int(eval_str("(car (range 1 4))")) == 1, "range with a non-zero start");
    TEST(gl_value_get_int(eval_str("(len (range 0 10 3))")) == 4, "range with a step");
    TEST(gl_value_get_int(eval_str("(car (range 0 10 3))")) == 0, "range step starts correctly");
    TEST(gl_value_get_bool(eval_str("(= (range 5 0 -2) (list 5 3 1))")) == 1, "range counts down");
    TEST(eval_str("(range 0 0)")->type == GL_VAL_NIL, "empty range");
    TEST(eval_expr("(range 0 5 0)") == NULL, "range rejects a zero step");
    TEST(gl_value_get_int(eval_str("(len (map 1+ (range 0 100)))")) == 100, "range feeds map");
}

void test_list_helpers(void) {
    TEST(gl_value_get_int(eval_str("(len (take (list 1 2 3 4) 2))")) == 2, "take length");
    TEST(gl_value_get_int(eval_str("(car (take (list 1 2 3) 5))")) == 1, "take clamps to length");
    TEST(eval_str("(take (list 1 2) 0)")->type == GL_VAL_NIL, "take of zero");
    TEST(eval_str("(take nil 3)")->type == GL_VAL_NIL, "take of nil");

    TEST(gl_value_get_int(eval_str("(car (drop (list 1 2 3 4) 2))")) == 3, "drop first element");
    TEST(gl_value_get_int(eval_str("(len (drop (list 1 2 3 4) 2))")) == 2, "drop length");
    TEST(eval_str("(drop (list 1 2) 5)")->type == GL_VAL_NIL, "drop past the end");

    TEST(gl_value_get_int(eval_str("(len (zip (list 1 2 3) (list \"a\" \"b\")))")) == 2,
         "zip stops at the shorter list");
    TEST(gl_value_get_int(eval_str("(car (car (zip (list 1 2) (list 3 4))))")) == 1,
         "zip first of the first pair");
    TEST(gl_value_get_int(eval_str("(car (cdr (car (zip (list 1 2) (list 3 4)))))")) == 3,
         "zip second of the first pair");

    TEST(gl_value_get_int(eval_str("(len (flatten (list 1 (list 2 (list 3 4)) 5)))")) == 5,
         "flatten nested lists");
    TEST(gl_value_get_int(eval_str("(car (flatten (list (list 1 2) 3)))")) == 1,
         "flatten keeps order");
}
// The list is longer than the evaluator's recursion limit, so these helpers have
// to use loops: a recursive map or filter would stop with a depth error.
void test_long_lists(void) {
    eval_str("(defun build-list (n acc) (if (= n 0) acc (build-list (- n 1) (cons n acc))))");
    eval_str("(set big-list (build-list 1200 nil))");

    TEST(gl_value_get_int(eval_str("(len big-list)")) == 1200, "long list length");
    TEST(gl_value_get_int(eval_str("(len (map 1+ big-list))")) == 1200, "map over a long list");
    TEST(gl_value_get_int(eval_str("(car (map 1+ big-list))")) == 2, "map keeps the order");
    TEST(gl_value_get_int(eval_str("(len (filter even? big-list))")) == 600,
         "filter over a long list");
    TEST(gl_value_get_int(eval_str("(len (append big-list (list 1 2)))")) == 1202,
         "append a long list");
    TEST(gl_value_get_int(eval_str("(car (member 1100 big-list))")) == 1100,
         "member of a long list");
    TEST(gl_value_get_bool(eval_str("(all? (lambda (x) t) big-list)")) == 1,
         "all? over a long list");
}

void test_alist(void) {
    eval_str("(set sample-alist (list (cons \"a\" 1) (cons \"b\" 2)))");

    TEST(gl_value_get_int(eval_str("(cdr (assoc \"a\" sample-alist))")) == 1, "assoc finds a key");
    TEST(eval_str("(assoc \"z\" sample-alist)")->type == GL_VAL_NIL, "assoc miss is nil");
    TEST(gl_value_get_int(eval_str("(alist-get \"b\" sample-alist)")) == 2, "alist-get value");
    TEST(gl_value_get_int(eval_str("(alist-get \"z\" sample-alist 99)")) == 99,
         "alist-get default");
    TEST(eval_str("(alist-get \"z\" sample-alist)")->type == GL_VAL_NIL,
         "alist-get without default");
    TEST(gl_value_get_int(eval_str("(len (alist-delete \"a\" sample-alist))")) == 1,
         "alist-delete removes the pair");
    TEST(strcmp(gl_value_get_string(eval_str("(car (car (alist-delete \"a\" sample-alist)))")),
                "b") == 0,
         "alist-delete keeps the rest");
    TEST(strcmp(gl_value_get_string(
                    eval_str("(car (cdr (assoc 2 (list (list 1 \"one\") (list 2 \"two\")))))")),
                "two") == 0,
         "assoc compares with the language equality");
}

void test_hash_helpers(void) {
    eval_str("(set helper-map (hash))");
    eval_str("(hash-set helper-map \"a\" 1)");
    eval_str("(hash-set helper-map \"b\" 2)");

    TEST(gl_value_get_bool(eval_str("(hash? helper-map)")) == 1, "hash? on a map");
    TEST(gl_value_get_bool(eval_str("(hash? (list 1))")) == 0, "hash? on a list");

    TEST(gl_value_get_int(eval_str("(len (hash-values helper-map))")) == 2, "hash-values length");
    TEST(gl_value_get_int(eval_str("(len (hash->alist helper-map))")) == 2, "hash->alist length");
    TEST(gl_value_get_int(eval_str("(cdr (car (hash->alist helper-map)))")) >= 1,
         "hash->alist pairs hold values");

    eval_str("(set helper-sum 0)");
    eval_str("(hash-each (lambda (k v) (set helper-sum (+ helper-sum v))) helper-map)");
    TEST(gl_value_get_int(eval_str("helper-sum")) == 3, "hash-each visits every value");
    TEST(eval_str("(hash-each (lambda (k v) v) helper-map)")->type == GL_VAL_NIL,
         "hash-each returns nil");

    TEST(gl_value_get_int(eval_str("(len (sort-by car (hash->alist helper-map)))")) == 2,
         "hash entries can be sorted");

    eval_str("(set merge-left (hash))");
    eval_str("(hash-set merge-left \"x\" 1)");
    eval_str("(hash-set merge-left \"y\" 2)");
    eval_str("(set merge-right (hash))");
    eval_str("(hash-set merge-right \"y\" 20)");
    eval_str("(hash-set merge-right \"z\" 3)");
    eval_str("(set merged (hash-merge merge-left merge-right))");

    TEST(gl_value_get_int(eval_str("(len merged)")) == 3, "hash-merge entry count");
    TEST(gl_value_get_int(eval_str("(alist-get \"y\" (hash->alist merged))")) == 20,
         "hash-merge prefers the later map");
    TEST(gl_value_get_int(eval_str("(alist-get \"x\" (hash->alist merged))")) == 1,
         "hash-merge keeps earlier keys");
    TEST(gl_value_get_int(eval_str("(len merge-left)")) == 2, "hash-merge leaves its inputs alone");
    TEST(gl_value_get_int(eval_str("(len (hash-merge))")) == 0, "hash-merge of nothing");

    eval_str("(set copy-source (hash))");
    eval_str("(hash-set copy-source \"k\" 1)");
    eval_str("(set copy-target (hash-copy copy-source))");
    eval_str("(hash-set copy-target \"extra\" 2)");
    TEST(gl_value_get_int(eval_str("(len copy-source)")) == 1,
         "hash-copy does not alias the source");
    TEST(gl_value_get_int(eval_str("(len copy-target)")) == 2,
         "hash-copy is independently mutable");
}

void test_sort(void) {
    TEST(gl_value_get_int(eval_str("(get (sort [3 1 2]) 0)")) == 1, "sort array");
    TEST(gl_value_get_int(eval_str("(get (sort [3 1 2]) 2)")) == 3, "sort array last element");

    TEST(strcmp(gl_value_get_string(
                    eval_str("(car (sort-by (lambda (s) (len s)) (list \"ccc\" \"a\" \"bb\")))")),
                "a") == 0,
         "sort-by orders by the key");
    TEST(gl_value_get_int(
             eval_str("(car (car (sort-by car (list (list 2 \"b\") (list 1 \"a\")))))")) == 1,
         "sort-by with a builtin key");
    TEST(gl_value_get_bool(eval_str("(= (sort-by (lambda (x) 0) (list 3 1 2)) (list 3 1 2))")) == 1,
         "sort-by keeps equal keys in order");
    TEST(gl_value_get_int(eval_str("(get (sort-by (lambda (x) (- 0 x)) [3 1 2]) 0)")) == 3,
         "sort-by preserves the array type");
    TEST(eval_str("(sort-by identity nil)")->type == GL_VAL_NIL, "sort-by of nil");

    TEST(gl_value_get_int(eval_str("(car (sort (list 3 1 2) >))")) == 3, "sort with a predicate");
    TEST(gl_value_get_int(eval_str("(car (sort (list 1 2 3) (lambda (a b) nil)))")) == 1,
         "sort with an all-false predicate keeps the order");
    TEST(strcmp(gl_value_get_string(eval_str("(car (sort (list \"bb\" \"a\" \"ccc\") "
                                             "(lambda (a b) (< (len a) (len b)))))")),
                "a") == 0,
         "sort compares by a computed key");
    TEST(gl_value_get_int(eval_str("(car (car (sort (list (list 2 \"b\") (list 1 \"a\")) "
                                   "(lambda (x y) (< (car x) (car y))))))")) == 1,
         "sort with a predicate over records");
    TEST(eval_expr("(sort (list 1 2) (lambda (a b) (car 1)))") == NULL,
         "a failing predicate aborts the sort");
    TEST(gl_value_get_int(eval_str("(len (sort nil >))")) == 0, "sort of nil with a predicate");
    TEST(gl_value_get_int(eval_str("(car (sort '(3 1 2)))")) == 1, "sort list");
    TEST(gl_value_get_int(eval_str("(get (sort {5 4 6}) 1)")) == 5, "sort vector");
    TEST(strcmp(gl_value_get_string(eval_str("(get (sort (list \"b\" \"a\" \"c\")) 0)")), "a") == 0,
         "sort strings");
    TEST(eval_str("(sort nil)")->type == GL_VAL_NIL, "sort nil");
    TEST(gl_value_get_int(eval_str("(let ((v {3 1 2})) (sort v) (get v 0))")) == 3,
         "sort leaves the source collection unchanged");
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

void test_cons(void) {
    gl_value_t *c1 = eval_str("(cons 1 2)");
    TEST(c1 != NULL && c1->type == GL_VAL_CONS, "cons creates cons cell");
    TEST(gl_value_get_int(eval_str("(car (cons 1 2))")) == 1, "car of cons");
    TEST(gl_value_get_int(eval_str("(cdr (cons 1 2))")) == 2, "cdr of cons");

    TEST(gl_value_get_int(eval_str("(car (cons 42 nil))")) == 42, "car of cons with nil");
    TEST(eval_str("(cdr (cons 42 nil))")->type == GL_VAL_NIL, "cdr of cons with nil is nil");
}

void test_apply(void) {
    TEST(gl_value_get_int(eval_str("(apply + [1 2 3])")) == 6, "apply + on array");
    TEST(gl_value_get_int(eval_str("(apply * {2 3 4})")) == 24, "apply * on vector");
    TEST(gl_value_get_int(eval_str("(apply + 10 20 [1 2 3])")) == 36, "apply + with prefix args");
    TEST(gl_value_get_int(eval_str("(apply + 5 {10 20})")) == 35, "apply + with vector and prefix");

    eval_str("(defun test-sub (a b) (- a b))");
    TEST(gl_value_get_int(eval_str("(apply test-sub [10 3])")) == 7,
         "apply user function on array");
    TEST(gl_value_get_int(eval_str("(apply test-sub (list 10 3))")) == 7,
         "apply user function on list");
}

void test_read_eval(void) {
    TEST(gl_value_get_int(eval_str("(eval '(+ 1 2))")) == 3, "Eval of quoted form");

    TEST(gl_value_get_int(eval_str("(eval (read \"(+ 1 2)\"))")) == 3, "Eval of read form");

    char *printed = gl_value_to_string(eval_str("(read \"'(1 2)\")"));
    TEST(strcmp(printed, "(quote (1 2))") == 0, "Read keeps quote as data");
    free(printed);

    printed = gl_value_to_string(eval_str("(read \"#'f\")"));
    TEST(strcmp(printed, "(function f)") == 0, "Read keeps function sugar as data");
    free(printed);

    gl_value_t *arr = eval_str("(read \"[1 2]\")");
    TEST(arr->type == GL_VAL_ARRAY, "Read array type");
    TEST(gl_value_get_array(arr)->size == 2, "Read array size");
    TEST(gl_value_get_int(gl_value_get_array(arr)->data[1]) == 2, "Read array element");

    TEST(eval_str("(read \"{1 2}\")")->type == GL_VAL_VECTOR, "Read vector type");

    TEST(eval_str("(eval (read \"(defun sq (x) (* x x))\"))") != NULL, "Eval defines a function");
    TEST(gl_value_get_int(eval_str("(eval (read \"(sq 5)\"))")) == 25,
         "Eval calls a defined function");

    TEST(gl_value_get_int(eval_str("(let ((x 4)) (eval (read \"(* x x)\")))")) == 16,
         "Eval sees let binding");

    TEST(gl_value_get_bool(eval_str("(eq (eval \"a\\\\b\") \"a\\\\b\")")) == 1,
         "Eval preserves string contents");

    printed = gl_value_to_string(eval_str("(read \"(1 (2 3) [4 5] {6})\")"));
    TEST(strcmp(printed, "(1 (2 3) [4 5] {6})") == 0, "Read preserves nested syntax");
    free(printed);

    TEST(gl_value_get_int(eval_str("(get (eval (read \"[1 (+ 1 2)]\")) 1)")) == 3,
         "Eval of a read array");
    TEST(gl_value_get_bool(eval_str("(= (eval (read \"{1 2}\")) {1 2})")) == 1,
         "Eval of a read vector");
    TEST(eval_expr("(eval car)") == NULL, "Eval rejects a builtin");

    TEST(eval_str("(read \"\")")->type == GL_VAL_NIL, "Empty input reads as nil");
    TEST(gl_value_get_int(eval_str("(read \"1 2\")")) == 1, "Trailing text is ignored");

    TEST(eval_expr("(read \"(1 2\")") == NULL, "Read reports a syntax error");
    TEST(eval_expr("(eval sq)") == NULL, "Eval rejects a function");
}

void test_quasiquote(void) {
    TEST(gl_value_get_bool(eval_str("(= `(1 2 3) '(1 2 3))")) == 1, "quasiquote literal list");
    TEST(eval_str("`()")->type == GL_VAL_NIL, "quasiquote empty list");

    eval_str("(set qq-x 10)");
    TEST(gl_value_get_bool(eval_str("(= `(1 ,qq-x 3) '(1 10 3))")) == 1, "unquote");
    TEST(gl_value_get_bool(eval_str("(= `(1 ,@(list 2 3) 4) '(1 2 3 4))")) == 1,
         "unquote-splicing a list");
    TEST(gl_value_get_bool(eval_str("(= `(1 ,@[2 3] 4) '(1 2 3 4))")) == 1,
         "unquote-splicing an array");
    TEST(gl_value_get_bool(eval_str("(= `(1 ,@{2 3} 4) '(1 2 3 4))")) == 1,
         "unquote-splicing a vector");
    TEST(gl_value_get_bool(eval_str("(= `(1 ,@nil 2) '(1 2))")) == 1, "unquote-splicing nil");

    TEST(gl_value_get_bool(eval_str("(= `(a `(b ,qq-x)) '(a (quasiquote (b (unquote qq-x)))))")) ==
             1,
         "nested quasiquote keeps inner unquote as data");
    TEST(gl_value_get_bool(eval_str("(= `[,qq-x 2] [10 2])")) == 1, "unquote in an array");
    TEST(gl_value_get_bool(eval_str("(= `{,qq-x 2} {10 2})")) == 1, "unquote in a vector");
    TEST(gl_value_get_bool(eval_str("(= `[,@(list 1 2) 3] [1 2 3])")) == 1,
         "unquote-splicing in an array");

    TEST(eval_expr("`,@(list 1 2)") == NULL, "splicing outside a sequence fails");
    TEST(eval_expr(",qq-x") == NULL, "unquote outside quasiquote fails");
    TEST(eval_expr(",@(list 1)") == NULL, "unquote-splicing outside quasiquote fails");
    TEST(eval_expr("`(1 ,@42)") == NULL, "splicing a non-sequence fails");
    TEST(eval_expr("`(1 ,@(cons 1 2))") == NULL, "splicing an improper list fails");
}

void test_macros(void) {
    TEST(eval_str("(defmacro my-unless (test &rest body) `(if ,test nil (progn ,@body)))") != NULL,
         "defmacro defines a macro");
    TEST(gl_value_get_int(eval_str("(my-unless nil 1 2)")) == 2, "macro expands and evaluates");
    TEST(gl_value_get_int(eval_str("(let ((n 5)) (my-unless nil n))")) == 5,
         "macro expansion sees the caller environment");

    TEST(gl_value_get_bool(eval_str("(macro? my-unless)")) == 1, "macro? on a macro");
    TEST(gl_value_get_bool(eval_str("(function? my-unless)")) == 0, "macro is not a function");
    TEST(gl_value_get_bool(eval_str("(= (typeof my-unless) macro-t)")) == 1, "typeof macro");
    TEST(gl_value_get_bool(eval_str("(= my-unless my-unless)")) == 1, "macro identity equality");
    TEST(gl_value_get_bool(eval_str("(macro? (id my-unless))")) == 1,
         "copying a macro preserves its type");

    char *printed = gl_value_to_string(eval_str("(macroexpand-1 '(my-unless nil 1 2))"));
    TEST(strcmp(printed, "(if nil nil (progn 1 2))") == 0, "macroexpand-1 expands one step");
    free(printed);

    printed = gl_value_to_string(eval_str("(macroexpand-1 '(1 2))"));
    TEST(strcmp(printed, "(1 2)") == 0, "macroexpand-1 leaves non-macros alone");
    free(printed);

    eval_str("(defmacro m1 (x) `(m2 ,x))");
    eval_str("(defmacro m2 (x) `(+ ,x 1))");
    printed = gl_value_to_string(eval_str("(macroexpand '(m1 41))"));
    TEST(strcmp(printed, "(+ 41 1)") == 0, "macroexpand expands repeatedly");
    free(printed);
    TEST(gl_value_get_int(eval_str("(m1 41)")) == 42, "nested macro evaluates");

    eval_str("(defmacro with-temp (value &rest body) "
             "  (let ((tmp (gensym \"tmp\"))) `(let ((,tmp ,value)) ,@body)))");
    TEST(gl_value_get_int(eval_str("(let ((tmp 7)) (with-temp 1 tmp))")) == 7,
         "gensym prevents macro capture");
    TEST(gl_value_get_int(eval_str("(with-temp 5 (+ 1 2))")) == 3, "macro body forms");

    TEST(gl_value_get_int(
             eval_str("(let ((n 3)) (defmacro local-m (x) `(+ ,x 10)) (local-m n))")) == 13,
         "local macro is callable in its scope");
    TEST(eval_expr("(local-m 1)") == NULL, "local macro is not visible outside its scope");

    TEST(eval_expr("(apply my-unless (list nil 1))") == NULL, "apply rejects a macro");

    eval_str("(defmacro no-args () 1)");
    TEST(eval_expr("(no-args 2)") == NULL, "macro arity is checked");

    eval_str("(defmacro endless-macro () '(endless-macro))");
    TEST(eval_expr("(endless-macro)") == NULL, "runaway macro expansion reports an error");

    eval_str("(defmacro endless-tail () '(endless-tail))");
    eval_str("(defun tail-macro-loop () (endless-tail))");
    TEST(eval_expr("(tail-macro-loop)") == NULL, "runaway tail macro expansion reports an error");
}

void test_load_relative(void) {
    gl_value_t *r1 = eval_str("(load \"tests/load_test/top.lisp\")");
    TEST(r1 != NULL, "load top.lisp succeeds");
    TEST(gl_value_get_int(eval_str("(top-marker)")) == 12346,
         "top.lisp successfully loaded nested/sub.lisp relatively");

    gl_value_t *r2 = eval_str("(load \"tests/load_test/nested/relative_up.lisp\")");
    TEST(r2 != NULL, "load relative_up.lisp succeeds");
    TEST(gl_value_get_int(eval_str("(sibling-marker)")) == 999,
         "relative_up.lisp successfully loaded ../sibling.lisp relatively");
}

void test_load_failure(void) {
    char path[512];
    int created = gl_test_make_temp(path, sizeof(path));
    TEST(created == 0, "create temporary load file");
    if (created != 0) {
        return;
    }

    FILE *file = fopen(path, "w");
    TEST(file != NULL, "write temporary load file");
    if (!file) {
        remove(path);
        return;
    }
    fputs("(undefined-function 1)", file);
    fclose(file);

    char src[512];
    snprintf(src, sizeof(src), "(load \"%s\")", path);
    TEST(eval_str(src) == NULL, "failed load returns NULL");
    TEST(!gl_call_stack_initialized || gl_call_stack.size == 0, "failed load leaves no call frame");
    TEST(gl_value_get_int(eval_str("(+ 1 2)")) == 3, "evaluation after failed load");

    file = fopen(path, "w");
    TEST(file != NULL, "rewrite temporary load file");
    if (file) {
        fputs("(set after-load 7)", file);
        fclose(file);
    }
    TEST(eval_str(src) != NULL, "load after failed load");
    TEST(gl_value_get_int(eval_str("after-load")) == 7, "later load evaluated");

    remove(path);
}

void test_stdlib(void) {
    TEST(gl_load_stdlib() == 0, "gl_load_stdlib succeeds");

    // Core predicates
    TEST(gl_value_get_bool(eval_str("(null? nil)")) == 1, "null? on nil");
    TEST(gl_value_get_bool(eval_str("(null? 1)")) == 0, "null? on non-nil");
    TEST(gl_value_get_bool(eval_str("(not t)")) == 0, "not t");
    TEST(gl_value_get_bool(eval_str("(not nil)")) == 1, "not nil");
    TEST(gl_value_get_bool(eval_str("(zero? 0)")) == 1, "zero? on 0");
    TEST(gl_value_get_bool(eval_str("(zero? 5)")) == 0, "zero? on 5");
    TEST(gl_value_get_bool(eval_str("(pos? 5)")) == 1, "pos? on 5");
    TEST(gl_value_get_bool(eval_str("(pos? -5)")) == 0, "pos? on -5");
    TEST(gl_value_get_bool(eval_str("(neg? -5)")) == 1, "neg? on -5");
    TEST(gl_value_get_bool(eval_str("(neg? 5)")) == 0, "neg? on 5");
    TEST(gl_value_get_bool(eval_str("(even? 4)")) == 1, "even? on 4");
    TEST(gl_value_get_bool(eval_str("(even? 5)")) == 0, "even? on 5");
    TEST(gl_value_get_bool(eval_str("(odd? 5)")) == 1, "odd? on 5");
    TEST(gl_value_get_bool(eval_str("(odd? 4)")) == 0, "odd? on 4");
    TEST(gl_value_get_int(eval_str("(id 42)")) == 42, "id function");

    // Math
    TEST(gl_value_get_int(eval_str("(1+ 5)")) == 6, "1+");
    TEST(gl_value_get_int(eval_str("(1- 5)")) == 4, "1-");
    TEST(gl_value_get_int(eval_str("(abs -10)")) == 10, "abs negative");
    TEST(gl_value_get_int(eval_str("(abs 10)")) == 10, "abs positive");
    TEST(gl_value_get_int(eval_str("(min 3 7)")) == 3, "min");
    TEST(gl_value_get_int(eval_str("(max 3 7)")) == 7, "max");
    TEST(gl_value_get_int(eval_str("(clamp 5 10 20)")) == 10, "clamp lower");
    TEST(gl_value_get_int(eval_str("(clamp 15 10 20)")) == 15, "clamp middle");
    TEST(gl_value_get_int(eval_str("(clamp 25 10 20)")) == 20, "clamp upper");

    // List
    TEST(gl_value_get_int(eval_str("(cadr (list 1 2 3))")) == 2, "cadr");
    TEST(gl_value_get_int(eval_str("(caddr (list 1 2 3))")) == 3, "caddr");
    TEST(gl_value_get_bool(eval_str("(= (reverse (list 1 2 3)) (list 3 2 1))")) == 1, "reverse");
    TEST(gl_value_get_bool(eval_str("(= (append (list 1 2) (list 3 4)) (list 1 2 3 4))")) == 1,
         "append");
    TEST(gl_value_get_int(eval_str("(car (member 2 (list 1 2 3)))")) == 2, "member found");
    TEST(eval_str("(member 5 (list 1 2 3))")->type == GL_VAL_NIL, "member not found");

    // Conversions
    TEST(gl_value_get_bool(eval_str("(= (to-array (list 1 2 3)) [1 2 3])")) == 1, "to-array");
    TEST(gl_value_get_bool(eval_str("(= (to-vector [1 2 3]) {1 2 3})")) == 1, "to-vector");
    TEST(gl_value_get_bool(eval_str("(= (to-list [1 2 3]) (list 1 2 3))")) == 1, "to-list");

    // Higher-order functions
    // map
    TEST(gl_value_get_bool(eval_str("(= (map 1+ (list 1 2 3)) (list 2 3 4))")) == 1, "map on list");
    TEST(gl_value_get_bool(eval_str("(= (map 1+ [1 2 3]) [2 3 4])")) == 1, "map on array");
    TEST(gl_value_get_bool(eval_str("(= (map 1+ {1 2 3}) {2 3 4})")) == 1, "map on vector");

    // filter
    TEST(gl_value_get_bool(eval_str("(= (filter even? (list 1 2 3 4 5 6)) (list 2 4 6))")) == 1,
         "filter on list");
    TEST(gl_value_get_bool(eval_str("(= (filter even? [1 2 3 4 5 6]) [2 4 6])")) == 1,
         "filter on array");
    TEST(gl_value_get_bool(eval_str("(= (filter odd? {1 2 3 4 5 6}) {1 3 5})")) == 1,
         "filter on vector");

    // reduce
    TEST(gl_value_get_int(eval_str("(reduce + (list 1 2 3 4) 0)")) == 10, "reduce + on list");
    TEST(gl_value_get_int(eval_str("(reduce * [1 2 3 4] 1)")) == 24, "reduce * on array");
    TEST(gl_value_get_int(eval_str("(reduce min {5 2 8 1 9} 999)")) == 1, "reduce min on vector");

    // any? / all?
    TEST(gl_value_get_bool(eval_str("(any? even? [1 3 5 6])")) == 1, "any? true");
    TEST(gl_value_get_bool(eval_str("(any? even? [1 3 5 7])")) == 0, "any? false");
    TEST(gl_value_get_bool(eval_str("(all? even? [2 4 6 8])")) == 1, "all? true");
    TEST(gl_value_get_bool(eval_str("(all? even? [2 4 5 8])")) == 0, "all? false");
}

void test_pragma(void) {
    // gc-allocated returns integer >= 0
    gl_value_t *alloc = eval_str("(pragma gc-allocated)");
    TEST(alloc != NULL && alloc->type == GL_VAL_INT, "pragma gc-allocated returns int");
    TEST(gl_value_get_int(alloc) >= 0, "pragma gc-allocated >= 0");

    // gc-collect returns nil
    gl_value_t *coll = eval_str("(pragma gc-collect)");
    TEST(coll != NULL && coll->type == GL_VAL_NIL, "pragma gc-collect returns nil");

    // gc-enabled?
    TEST(gl_value_get_bool(eval_str("(pragma gc-enabled?)")) == 1,
         "pragma gc-enabled? initially true");

    // gc-toggle without args flips flag
    TEST(gl_value_get_bool(eval_str("(pragma gc-toggle)")) == 0, "pragma gc-toggle flips to false");
    TEST(gl_value_get_bool(eval_str("(pragma gc-enabled?)")) == 0, "pragma gc-enabled? is false");
    TEST(gl_value_get_bool(eval_str("(pragma gc-toggle)")) == 1,
         "pragma gc-toggle flips back to true");
    TEST(gl_value_get_bool(eval_str("(pragma gc-enabled?)")) == 1, "pragma gc-enabled? is true");

    // gc-toggle with arg sets flag explicitly
    TEST(gl_value_get_bool(eval_str("(pragma gc-toggle nil)")) == 0, "pragma gc-toggle nil");
    TEST(gl_value_get_bool(eval_str("(pragma gc-enabled?)")) == 0, "pragma gc-enabled? after nil");
    TEST(gl_value_get_bool(eval_str("(pragma gc-toggle t)")) == 1, "pragma gc-toggle t");
    TEST(gl_value_get_bool(eval_str("(pragma gc-enabled?)")) == 1, "pragma gc-enabled? after t");
}

void test_cond(void) {
    // Empty cond returns nil
    gl_value_t *v_empty = eval_str("(cond)");
    TEST(v_empty != NULL && v_empty->type == GL_VAL_NIL, "cond empty");

    // Single matching clause
    gl_value_t *v1 = eval_str("(cond (t 42))");
    TEST(v1 != NULL && gl_value_get_int(v1) == 42, "cond (t 42)");

    // First matching clause chosen
    gl_value_t *v2 = eval_str("(cond ((= 1 1) \"first\") ((= 2 2) \"second\"))");
    TEST(v2 != NULL && strcmp(v2->val, "first") == 0, "cond first match");

    // Second matching clause chosen
    gl_value_t *v3 = eval_str("(cond ((= 1 2) \"first\") ((= 2 2) \"second\"))");
    TEST(v3 != NULL && strcmp(v3->val, "second") == 0, "cond second match");

    // No matching clause returns nil
    gl_value_t *v_nomatch = eval_str("(cond ((= 1 2) 10) ((= 3 4) 20))");
    TEST(v_nomatch != NULL && v_nomatch->type == GL_VAL_NIL, "cond no match returns nil");

    // 'else' clause
    gl_value_t *v_else = eval_str("(cond ((= 1 2) \"no\") (else \"yes\"))");
    TEST(v_else != NULL && strcmp(v_else->val, "yes") == 0, "cond else clause");

    // Single-expression clause returns condition value
    gl_value_t *v_single = eval_str("(cond (nil) (55))");
    TEST(v_single != NULL && gl_value_get_int(v_single) == 55, "cond single-expression clause");

    // Multi-expression clause body (evaluated in sequence)
    gl_value_t *v_multi = eval_str("(let ((x 1)) (cond (t (set x (+ x 10)) (set x (* x 2)) x)))");
    TEST(v_multi != NULL && gl_value_get_int(v_multi) == 22, "cond multi-expression body");

    // Short-circuit: false branch expressions are not evaluated
    eval_str("(set test_flag 0)");
    eval_str("(cond (nil (set test_flag 99)) (t (set test_flag 1)))");
    TEST(gl_value_get_int(eval_str("test_flag")) == 1, "cond short-circuit");
}

void test_optional_and_rest(void) {
    // 1. Bare &optional parameter defaults to nil
    eval_str("(defun opt1 (a &optional b) (list a b))");
    TEST(gl_value_get_bool(eval_str("(= (opt1 1) (list 1 nil))")) == 1, "opt bare omitted");
    TEST(gl_value_get_bool(eval_str("(= (opt1 1 2) (list 1 2))")) == 1, "opt bare provided");

    // 2. &optional with default value
    eval_str("(defun opt2 (a &optional (b 10)) (+ a b))");
    TEST(gl_value_get_int(eval_str("(opt2 5)")) == 15, "opt default omitted");
    TEST(gl_value_get_int(eval_str("(opt2 5 20)")) == 25, "opt default provided");

    // 3. &optional default referencing earlier parameter
    eval_str("(defun rect (w &optional (h w)) (list w h))");
    TEST(gl_value_get_bool(eval_str("(= (rect 7) (list 7 7))")) == 1,
         "opt default referencing param");
    TEST(gl_value_get_bool(eval_str("(= (rect 7 3) (list 7 3))")) == 1, "opt default overridden");

    // 4. Multiple &optional parameters
    eval_str("(defun multi-opt (a &optional (b 2) (c 3)) (+ a (* b c)))");
    TEST(gl_value_get_int(eval_str("(multi-opt 1)")) == 7, "multi-opt 1 arg");
    TEST(gl_value_get_int(eval_str("(multi-opt 1 10)")) == 31, "multi-opt 2 args");
    TEST(gl_value_get_int(eval_str("(multi-opt 1 10 5)")) == 51, "multi-opt 3 args");

    // 5. Basic &rest
    eval_str("(defun rest-fn (&rest args) args)");
    TEST(eval_str("(rest-fn)")->type == GL_VAL_NIL, "rest empty is nil");
    TEST(gl_value_get_bool(eval_str("(= (rest-fn 1 2 3) (list 1 2 3))")) == 1,
         "rest non-empty list");

    // 6. Required + &rest
    eval_str("(defun req-rest (first &rest others) (list first others))");
    TEST(gl_value_get_bool(eval_str("(= (req-rest 10) (list 10 nil))")) == 1, "req-rest no rest");
    TEST(gl_value_get_bool(eval_str("(= (req-rest 10 20 30) (list 10 (list 20 30)))")) == 1,
         "req-rest with rest");

    // 7. Required + &optional + &rest
    eval_str("(defun full-fn (a &optional (b 100) &rest c) (list a b c))");
    TEST(gl_value_get_bool(eval_str("(= (full-fn 1) (list 1 100 nil))")) == 1, "full-fn 1 arg");
    TEST(gl_value_get_bool(eval_str("(= (full-fn 1 2) (list 1 2 nil))")) == 1, "full-fn 2 args");
    TEST(gl_value_get_bool(eval_str("(= (full-fn 1 2 3 4) (list 1 2 (list 3 4)))")) == 1,
         "full-fn 4 args");

    // 8. Anonymous lambda with &optional and &rest
    TEST(gl_value_get_int(eval_str("((lambda (x &optional (y 5)) (+ x y)) 10)")) == 15,
         "lambda &optional");
    TEST(gl_value_get_bool(eval_str("(= ((lambda (&rest r) r) 1 2) (list 1 2))")) == 1,
         "lambda &rest");

    // 9. apply with &rest function
    TEST(gl_value_get_bool(eval_str("(= (apply rest-fn [1 2 3 4]) (list 1 2 3 4))")) == 1,
         "apply with &rest");
}

void test_for(void) {
    // 1. Iterating over list
    eval_str("(set sum 0)");
    eval_str("(for (x (list 1 2 3 4)) (set sum (+ sum x)))");
    TEST(gl_value_get_int(eval_str("sum")) == 10, "for over list");

    // 2. Iterating over array
    eval_str("(set sum 0)");
    eval_str("(for (x [10 20 30]) (set sum (+ sum x)))");
    TEST(gl_value_get_int(eval_str("sum")) == 60, "for over array");

    // 3. Iterating over vector
    eval_str("(set out {})");
    eval_str("(for (x {1 2 3}) (push out (* x 2)))");
    TEST(gl_value_get_bool(eval_str("(= out {2 4 6})")) == 1, "for over vector");

    // 4. Iterating over string
    eval_str("(set count 0)");
    eval_str("(for (ch \"hello\") (set count (+ count 1)))");
    TEST(gl_value_get_int(eval_str("count")) == 5, "for over string");

    // 5. Empty collections do not execute body
    eval_str("(set ran 0)");
    eval_str("(for (x nil) (set ran 1))");
    TEST(gl_value_get_int(eval_str("ran")) == 0, "for over nil");

    eval_str("(for (x []) (set ran 1))");
    TEST(gl_value_get_int(eval_str("ran")) == 0, "for over empty array");

    // 6. for returns nil
    gl_value_t *v_ret = eval_str("(for (x [1 2 3]) x)");
    TEST(v_ret != NULL && v_ret->type == GL_VAL_NIL, "for returns nil");

    // 7. Scoping: variable is local to for loop
    eval_str("(set i 999)");
    eval_str("(for (i [1 2 3]) nil)");
    TEST(gl_value_get_int(eval_str("i")) == 999, "for variable does not leak");
}

void test_update_macros(void) {
    eval_str("(set upd-counter 0)");
    TEST(gl_value_get_int(eval_str("(incf upd-counter)")) == 1, "incf returns the new value");
    TEST(gl_value_get_int(eval_str("upd-counter")) == 1, "incf updates the binding");
    TEST(gl_value_get_int(eval_str("(incf upd-counter 10)")) == 11, "incf with a delta");
    TEST(gl_value_get_int(eval_str("(decf upd-counter)")) == 10, "decf returns the new value");
    TEST(gl_value_get_int(eval_str("(decf upd-counter 4)")) == 6, "decf with a delta");
    TEST(gl_value_get_int(eval_str("upd-counter")) == 6, "decf updates the binding");

    // The macro expands to set, so it updates locals as well as globals.
    TEST(gl_value_get_int(eval_str("(let ((n 1)) (incf n 2) n)")) == 3, "incf on a let binding");
    eval_str("(defun bump (n) (incf n) n)");
    TEST(gl_value_get_int(eval_str("(bump 41)")) == 42, "incf on a parameter");

    // The delta is evaluated once, not once per mention.
    eval_str("(set upd-evals 0)");
    eval_str("(defun next-delta () (set upd-evals (+ upd-evals 1)) 5)");
    TEST(gl_value_get_int(eval_str("(let ((n 0)) (incf n (next-delta)) n)")) == 5,
         "incf applies the delta");
    TEST(gl_value_get_int(eval_str("upd-evals")) == 1, "incf evaluates the delta once");
}

void test_control_forms(void) {
    TEST(eval_expr("(let ((i 0)) (while (< i 3) (len 5)))") == NULL,
         "While stops when the body fails");
    TEST(eval_expr("(while (len 5) 0)") == NULL, "While stops when the condition fails");
    TEST(gl_value_get_int(eval_str("(let ((i 0)) (while (< i 3) (set i (+ i 1))) i)")) == 3,
         "While still runs normally");

    eval_str("(defun cond-count (n) (cond ((= n 0) 0) (else (cond-count (- n 1)))))");
    TEST(gl_value_get_int(eval_str("(cond-count 5000)")) == 0,
         "tail recursion through cond runs in constant stack");
    TEST(gl_value_get_int(eval_str("(when t 1 2 3)")) == 3, "When returns last body value");
    gl_value_t *when_nil = eval_str("(when nil 1)");
    TEST(when_nil != NULL && when_nil->type == GL_VAL_NIL, "When false returns nil");

    eval_str("(set when_ran 0)");
    eval_str("(when nil (set when_ran 1))");
    TEST(gl_value_get_int(eval_str("when_ran")) == 0, "When false skips body");

    TEST(gl_value_get_int(eval_str("(unless nil 1 2 3)")) == 3, "Unless returns last body value");
    gl_value_t *unless_nil = eval_str("(unless t 1)");
    TEST(unless_nil != NULL && unless_nil->type == GL_VAL_NIL, "Unless true returns nil");

    eval_str("(set unless_ran 0)");
    eval_str("(unless t (set unless_ran 1))");
    TEST(gl_value_get_int(eval_str("unless_ran")) == 0, "Unless true skips body");

    TEST(gl_value_get_int(eval_str("(case 2 (1 10) (2 20) (3 30))")) == 20, "Case matching key");
    TEST(gl_value_get_int(eval_str("(case 9 (1 10) (else 99))")) == 99, "Case else clause");
    gl_value_t *case_nil = eval_str("(case 9 (1 10) (2 20))");
    TEST(case_nil != NULL && case_nil->type == GL_VAL_NIL, "Case no match returns nil");
    TEST(gl_value_get_int(eval_str("(case 1 (1 10) (1 20))")) == 10, "Case first match wins");
    TEST(gl_value_get_int(eval_str("(case 2 ((+ 1 1) 20) (else 99))")) == 20, "Case evaluated key");

    eval_str("(set case_ran 0)");
    eval_str("(case 1 (1 (set case_ran 1)) (2 (set case_ran 2)))");
    TEST(gl_value_get_int(eval_str("case_ran")) == 1, "Case skips other bodies");

    eval_str("(set case_val_calls 0)");
    eval_str("(case (progn (set case_val_calls (+ case_val_calls 1)) 2) (1 10) (2 20))");
    TEST(gl_value_get_int(eval_str("case_val_calls")) == 1, "Case evaluates its value once");

    eval_str("(set case_key_calls 0)");
    TEST(gl_value_get_int(
             eval_str("(case 2 ((progn (set case_key_calls (+ case_key_calls 1)) 1) 10) "
                      "        ((progn (set case_key_calls (+ case_key_calls 1)) 2) 20) "
                      "        ((progn (set case_key_calls (+ case_key_calls 1)) 3) 30))")) == 20,
         "Case evaluates keys until the match");
    TEST(gl_value_get_int(eval_str("case_key_calls")) == 2, "Case skips later keys");

    eval_str("(set cond_calls 0)");
    TEST(gl_value_get_int(eval_str(
             "(cond ((progn (set cond_calls (+ cond_calls 1)) nil) 1) (else cond_calls))")) == 1,
         "Cond evaluates a test once");

    eval_str("(set dot_count_calls 0)");
    eval_str("(dotimes (i (progn (set dot_count_calls (+ dot_count_calls 1)) 3) nil))");
    TEST(gl_value_get_int(eval_str("dot_count_calls")) == 1, "Dotimes evaluates count once");

    eval_str("(set dot_seq nil)");
    eval_str("(dotimes (i 3) (set dot_seq (cons i dot_seq)))");
    TEST(gl_value_get_bool(eval_str("(= dot_seq (list 2 1 0))")) == 1, "Dotimes counter values");
    TEST(gl_value_get_int(eval_str("(dotimes (i 4 i))")) == 3, "Dotimes result sees final binding");

    eval_str("(set dot_ran 0)");
    gl_value_t *dot_nil = eval_str("(dotimes (i 0) (set dot_ran 1))");
    TEST(gl_value_get_int(eval_str("dot_ran")) == 0, "Dotimes zero count skips body");
    TEST(dot_nil != NULL && dot_nil->type == GL_VAL_NIL, "Dotimes zero count returns nil");
    TEST(gl_value_get_int(eval_str("(dotimes (i 0 i))")) == 0,
         "Dotimes zero count result sees binding");

    TEST(eval_str("(dotimes (i 1.5) nil)") == NULL, "Dotimes non-integer count fails");
    TEST(eval_str("(dotimes (i -1) nil)") == NULL, "Dotimes negative count fails");
    TEST(eval_str("(dotimes (t 1) nil)") == NULL, "Dotimes does not shadow constants");

    TEST(gl_value_get_int(eval_str("(let ((n 10)) (when t n))")) == 10, "When sees let binding");
    TEST(gl_value_get_int(eval_str("(let ((n 10)) (dotimes (i 1 n)))")) == 10,
         "Dotimes sees let binding");
    TEST(gl_value_get_int(eval_str("(let ((n 10)) (dotimes (i 1) (set n (+ n 1))) n)")) == 11,
         "Dotimes body sees let binding");
}

void test_add_to_list(void) {
    // 1. Add to uninitialized / nil variable
    eval_str("(set my_l nil)");
    eval_str("(add-to-list my_l 10)");
    TEST(gl_value_get_bool(eval_str("(= my_l (list 10))")) == 1, "add-to-list to nil");

    // 2. Add to beginning by default (cons)
    eval_str("(add-to-list my_l 20)");
    TEST(gl_value_get_bool(eval_str("(= my_l (list 20 10))")) == 1, "add-to-list prepends");

    // 3. Do not add duplicate
    eval_str("(add-to-list my_l 10)");
    TEST(gl_value_get_bool(eval_str("(= my_l (list 20 10))")) == 1,
         "add-to-list ignores duplicate");

    // 4. Add to end when append is true
    eval_str("(add-to-list my_l 30 t)");
    TEST(gl_value_get_bool(eval_str("(= my_l (list 20 10 30))")) == 1, "add-to-list appends");

    // 5. Quoted symbol syntax
    eval_str("(add-to-list 'my_l 40)");
    TEST(gl_value_get_bool(eval_str("(= my_l (list 40 20 10 30))")) == 1,
         "add-to-list with quoted symbol");

    // 6. Return value is the updated list
    gl_value_t *ret = eval_str("(add-to-list my_l 50)");
    TEST(ret != NULL && gl_value_get_bool(eval_str("(= my_l (list 50 40 20 10 30))")) == 1,
         "add-to-list returns updated list");

    // 7. Scoping: updates local binding in let
    eval_str("(set outer (list 1 2))");
    eval_str("(let ((outer (list 10 20))) (add-to-list outer 30) (assert (= outer (list 30 10 "
             "20))))");
    TEST(gl_value_get_bool(eval_str("(= outer (list 1 2))")) == 1,
         "add-to-list respects lexical scope");
}

void test_defconst(void) {
    // 1. Defining a constant and reading it
    gl_value_t *v = eval_str("(defconst pi 3.14159)");
    TEST(v != NULL && v->type == GL_VAL_FLOAT, "defconst evaluates and returns value");
    TEST(eval_str("pi") != NULL && gl_value_get_float(eval_str("pi")) > 3.14,
         "defconst variable access");

    // 2. defconst with optional docstring
    gl_value_t *v_doc = eval_str("(defconst MY_CONST 42 \"A docstring\")");
    TEST(v_doc != NULL && gl_value_get_int(eval_str("MY_CONST")) == 42, "defconst with docstring");

    // 3. defconstant alias
    gl_value_t *v_alias = eval_str("(defconstant e 2.71828)");
    TEST(v_alias != NULL && gl_value_get_float(eval_str("e")) > 2.7,
         "defconstant alias defines constant");

    // 4. Modifying constant via set fails
    TEST(eval_expr("(set PI 3.0)") == NULL, "set on constant fails");
    TEST(eval_expr("(set E 2.0)") == NULL, "set on defconstant fails");
    TEST(eval_expr("(set t nil)") == NULL, "set on built-in t fails");
    TEST(eval_expr("(set nil 1)") == NULL, "set on built-in nil fails");
    TEST(eval_expr("(set else nil)") == NULL, "set on built-in else fails");

    // 5. Modifying constant via set-global fails
    TEST(eval_expr("(set-global PI 3.0)") == NULL, "set-global on constant fails");
    TEST(eval_expr("(set-global t nil)") == NULL, "set-global on built-in t fails");
    TEST(eval_expr("(set-global int-t 42)") == NULL, "set-global on built-in typeid fails");

    // 6. Redefining constant via defconst fails
    TEST(eval_expr("(defconst PI 3.0)") == NULL, "re-defining constant fails");
    TEST(eval_expr("(defconst t 1)") == NULL, "defconst on built-in t fails");

    // 7. Modifying constant via add-to-list fails
    eval_str("(defconst CONST_LIST '(1 2))");
    TEST(eval_expr("(add-to-list 'CONST_LIST 3)") == NULL, "add-to-list on constant fails");
    TEST(eval_expr("(add-to-list 'nil 3)") == NULL, "add-to-list on nil fails");

    // 8. Binding constant in let / let* / for fails
    TEST(eval_expr("(let ((t 10)) t)") == NULL, "binding t in let fails");
    TEST(eval_expr("(let* ((nil 10)) nil)") == NULL, "binding nil in let* fails");
    TEST(eval_expr("(let ((PI 10)) PI)") == NULL, "binding constant in let fails");
    TEST(eval_expr("(for (t '(1 2 3)) t)") == NULL, "binding t in for fails");
    TEST(eval_expr("(for (PI '(1 2 3)) PI)") == NULL, "binding constant in for fails");

    // 9. Built-in constants remain unaffected
    TEST(gl_value_get_bool(eval_str("t")) == 1, "t is still true");
    TEST(eval_str("nil")->type == GL_VAL_NIL, "nil is still nil");
    TEST(eval_str("int-t")->type == GL_VAL_TYPEID, "int-t is still typeid");
}

void test_autoload_stdlib(void) {
    // 1. Loading from directory path in GLISP_STD_PATH
    gl_test_setenv("GLISP_STD_PATH", "std");
    TEST(gl_load_stdlib() == 0, "autoload std from directory in GLISP_STD_PATH");
    TEST(gl_value_get_int(eval_str("(id 42)")) == 42, "id function available after autoload");

    // 2. Loading from file path in GLISP_STD_PATH
    gl_test_setenv("GLISP_STD_PATH", "std/std.lisp");
    TEST(gl_load_stdlib() == 0, "autoload std from file in GLISP_STD_PATH");

    // 3. Fallback to GLISP_STD alias
    gl_test_unsetenv("GLISP_STD_PATH");
    gl_test_setenv("GLISP_STD", "std");
    TEST(gl_load_stdlib() == 0, "autoload std from GLISP_STD alias");

    // 4. Fallback without env vars (relative path or GLISP_SOURCE_DIR)
    gl_test_unsetenv("GLISP_STD");
    TEST(gl_load_stdlib() == 0, "autoload std from default fallback");

    // 5. Invalid path returns failure code (-1)
    gl_test_setenv("GLISP_STD_PATH", "nonexistent_dir_12345");
    TEST(gl_load_stdlib() == -1, "autoload std returns -1 on nonexistent path");
    gl_test_unsetenv("GLISP_STD_PATH");
}

void test_defvar(void) {
    TEST(gl_value_get_int(eval_str("(defvar dv-init 1) dv-init")) == 1,
         "defvar initializes a value");
    TEST(gl_value_get_int(eval_str("(defvar dv-init 2) dv-init")) == 1,
         "defvar keeps an existing value");
    TEST(gl_value_get_int(eval_str("(set dv-init 3) (defvar dv-init 4) dv-init")) == 3,
         "defvar keeps a later set");

    eval_str("(set dv-side 0)");
    TEST(gl_value_get_int(eval_str("(defvar dv-bound 10)")) == 10, "defvar binds a new value");
    TEST(gl_value_get_int(eval_str("(defvar dv-bound (set dv-side 1))")) == 10,
         "defvar returns an existing value");
    TEST(gl_value_get_int(eval_str("dv-side")) == 0,
         "defvar does not evaluate init when the variable is bound");

    gl_value_t *empty = eval_str("(defvar dv-empty)");
    TEST(empty != NULL && empty->type == GL_VAL_NIL, "defvar without init binds nil");
    TEST(gl_value_get_bool(eval_str("(nil? dv-empty)")) == 1, "uninitialized defvar is nil");

    eval_str("(set dv-outer 5)");
    TEST(gl_value_get_int(eval_str("(let ((dv-outer 7)) (defvar dv-from-local dv-outer))")) == 7,
         "defvar evaluates init in the current environment");
    TEST(gl_value_get_int(eval_str("dv-from-local")) == 7, "defvar from a local scope is global");

    eval_str("(defun dv-in-function () (defvar dv-global 42))");
    eval_str("(dv-in-function)");
    TEST(gl_value_get_int(eval_str("dv-global")) == 42,
         "defvar inside a function defines a global");

    eval_str("(set dv-shadow \"global\")");
    TEST(strcmp(gl_value_get_string(
                    eval_str("(let ((dv-shadow \"local\")) (defvar dv-shadow \"new\") dv-shadow)")),
                "local") == 0,
         "defvar leaves a local binding untouched");
    TEST(strcmp(gl_value_get_string(eval_str("dv-shadow")), "global") == 0,
         "defvar leaves an existing global value");

    eval_str("(defun dv-local-only () (let ((dv-only \"local\")) (defvar dv-only \"new\")))");
    eval_str("(dv-local-only)");
    TEST(strcmp(gl_value_get_string(eval_str("dv-only")), "new") == 0,
         "defvar defines a global even when a local variable shadows it");

    TEST(eval_expr("(defvar t 1)") == NULL, "defvar rejects constants");
    TEST(eval_expr("(defvar PI 1)") == NULL, "defvar rejects stdlib constants");
    TEST(eval_expr("(defvar 1 2)") == NULL, "defvar requires a symbol name");
    TEST(eval_expr("(defvar)") == NULL, "defvar requires a name");
    TEST(eval_expr("(defvar dv-bad 1 2)") == NULL, "defvar docstring must be a string");
    TEST(eval_expr("(defvar dv-bad 1 \"doc\" 2)") == NULL, "defvar rejects extra arguments");
    TEST(gl_value_get_int(eval_str("(defvar dv-doc 1 \"doc\")")) == 1,
         "defvar accepts a docstring");
}

void test_io(void) {
    char path[512];
    int created = gl_test_make_temp(path, sizeof(path));
    TEST(created == 0, "create temporary file");
    if (created != 0) {
        return;
    }

    char src[512];

    snprintf(src, sizeof(src), "(set io-file (open \"%s\" \"w+\"))", path);
    gl_value_t *file = eval_str(src);
    TEST(file != NULL && file->type == GL_VAL_FILE, "open file with w+ mode");
    if (file == NULL || file->type != GL_VAL_FILE) {
        remove(path);
        return;
    }
    TEST(gl_value_get_bool(eval_str("(open? io-file)")) == 1, "open? on open file");

    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == (GL_FILE_READ | GL_FILE_WRITE),
         "file-mode returns read|write");
    TEST(gl_value_get_bool(eval_str("(readable? io-file)")) == 1, "readable? on w+ file");
    TEST(gl_value_get_bool(eval_str("(writable? io-file)")) == 1, "writable? on w+ file");
    TEST(gl_value_get_bool(eval_str("(appendable? io-file)")) == 0, "appendable? on w+ file");
    TEST(gl_value_get_bool(eval_str("(file-mode? io-file (logior file-read file-write))")) == 1,
         "file-mode? matches read|write");

    TEST(gl_value_get_int(eval_str("(write-string \"hello\" io-file)")) == 5,
         "write-string returns bytes written");
    TEST(eval_str("(flush io-file)") != NULL, "flush on open file");

    TEST(gl_value_get_bool(eval_str("(close io-file)")) == 1, "close file");
    TEST(gl_value_get_bool(eval_str("(open? io-file)")) == 0, "open? on closed file");
    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == (GL_FILE_READ | GL_FILE_WRITE),
         "file-mode survives close");
    TEST(eval_expr("(eof? io-file)") == NULL, "eof? on closed file fails");
    TEST(eval_expr("(flush io-file)") == NULL, "flush on closed file fails");
    TEST(eval_expr("(open? 1)") == NULL, "open? rejects non-file");
    TEST(eval_expr("(eof? 1)") == NULL, "eof? rejects non-file");
    TEST(eval_expr("(file-mode 1)") == NULL, "file-mode rejects non-file");

    snprintf(src, sizeof(src), "(set io-file (open \"%s\" \"rb\"))", path);
    TEST(eval_str(src) != NULL, "open file with binary read mode");
    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == GL_FILE_READ,
         "file-mode returns read for rb");
    TEST(gl_value_get_bool(eval_str("(readable? io-file)")) == 1, "readable? on rb file");
    TEST(gl_value_get_bool(eval_str("(writable? io-file)")) == 0, "writable? on rb file");
    gl_value_t *line = eval_str("(read-line io-file)");
    TEST(line != NULL && line->type == GL_VAL_STRING &&
             strcmp(gl_value_get_string(line), "hello") == 0,
         "read back written contents");
    gl_value_t *eof_line = eval_str("(read-line io-file)");
    TEST(eof_line != NULL && eof_line->type == GL_VAL_NIL, "read-line returns nil at EOF");
    TEST(gl_value_get_bool(eval_str("(eof? io-file)")) == 1, "eof? is true after EOF");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open \"%s\" (logior file-read file-write)))", path);
    TEST(eval_str(src) != NULL, "open with integer read|write mode");
    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == (GL_FILE_READ | GL_FILE_WRITE),
         "file-mode from integer mode");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open-read \"%s\"))", path);
    TEST(eval_str(src) != NULL, "open-read helper");
    TEST(gl_value_get_bool(eval_str("(readable? io-file)")) == 1, "open-read is readable");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open-append \"%s\"))", path);
    TEST(eval_str(src) != NULL, "open-append helper");
    TEST(gl_value_get_bool(eval_str("(writable? io-file)")) == 1, "append file is writable");
    TEST(gl_value_get_bool(eval_str("(appendable? io-file)")) == 1, "append file is appendable");
    TEST(gl_value_get_bool(eval_str("(readable? io-file)")) == 0, "append file is not readable");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open-write \"%s\"))", path);
    TEST(eval_str(src) != NULL, "open-write helper");
    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == GL_FILE_WRITE,
         "open-write mode is write");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open-read-write \"%s\"))", path);
    TEST(eval_str(src) != NULL, "open-read-write helper");
    TEST(gl_value_get_int(eval_str("(file-mode io-file)")) == (GL_FILE_READ | GL_FILE_WRITE),
         "open-read-write mode is read|write");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(open \"%s\" 0)", path);
    TEST(eval_expr(src) == NULL, "open rejects zero integer mode");
    snprintf(src, sizeof(src), "(open \"%s\" 8)", path);
    TEST(eval_expr(src) == NULL, "open rejects unknown mode bits");

    TEST(gl_value_get_bool(eval_str("(function? printf)")) == 1, "printf is a function");
    TEST(gl_value_get_bool(eval_str("(function? fprintf)")) == 1, "fprintf is a function");
    TEST(gl_value_get_bool(eval_str("(function? printf-err)")) == 1, "printf-err is a function");

    snprintf(src, sizeof(src), "(set io-file (open-write \"%s\"))", path);
    TEST(eval_str(src) != NULL, "open file for fprintf test");
    TEST(eval_str("(fprintf io-file \"{} {}\\n\" 1 2)") != NULL, "fprintf writes formatted output");
    TEST(eval_str("(apply fprintf io-file \"{}\" (list 42))") != NULL,
         "fprintf works through apply");
    eval_str("(close io-file)");

    snprintf(src, sizeof(src), "(set io-file (open-read \"%s\"))", path);
    TEST(eval_str(src) != NULL, "reopen file after fprintf");
    gl_value_t *formatted = eval_str("(read-line io-file)");
    TEST(formatted != NULL && formatted->type == GL_VAL_STRING &&
             strcmp(gl_value_get_string(formatted), "1 2") == 0,
         "fprintf substitutes placeholders");
    gl_value_t *applied = eval_str("(read-line io-file)");
    TEST(applied != NULL && applied->type == GL_VAL_STRING &&
             strcmp(gl_value_get_string(applied), "42") == 0,
         "fprintf result through apply");
    eval_str("(close io-file)");

    remove(path);
}

void test_string_escapes(void) {
    TEST(gl_value_get_int(eval_str("(len \"a\\nb\")")) == 3, "newline escape");
    TEST(gl_value_get_int(eval_str("(len \"\\x41\\x42\")")) == 2, "hex escapes");
    TEST(strcmp(gl_value_get_string(eval_str("\"\\x41\\x42\"")), "AB") == 0, "hex escape value");
}

void test_gc_stress(void) {
    TEST(eval_str("(defun gc-default (&optional (x 7)) x)") != NULL,
         "define function for GC stress");
    TEST(eval_str("(set gc-i 0)") != NULL, "initialize GC stress counter");
    TEST(eval_str("(set gc-acc nil)") != NULL, "initialize GC stress accumulator");

    gl_value_t *loop = eval_str("(while (< gc-i 3000) (gc-default) "
                                "  (set gc-acc (cons gc-i gc-acc)) (set gc-i (+ gc-i 1)))");
    TEST(loop != NULL, "GC stress loop completes");

    TEST(gl_value_get_int(eval_str("gc-i")) == 3000, "GC stress counter value");
    TEST(gl_value_get_int(eval_str("(len gc-acc)")) == 3000, "GC stress accumulator length");
    TEST(gl_value_get_int(eval_str("(gc-default)")) == 7, "optional default survives GC");
}

void test_call_resolution(void) {
    TEST(gl_value_get_int(eval_str("(let ((f (lambda (x) (* x 2)))) (f 21))")) == 42,
         "local binding is callable");

    eval_str("(defun shadow-target (x) (+ x 1))");
    TEST(gl_value_get_int(
             eval_str("(let ((shadow-target (lambda (x) (* x 2)))) (shadow-target 21))")) == 42,
         "local binding shadows a global function");
    TEST(gl_value_get_int(eval_str("(shadow-target 41)")) == 42, "global function still works");

    eval_str("(defun f (x) x)");
    TEST(gl_value_get_int(eval_str("(car (map 1+ '(1 2)))")) == 2,
         "stdlib parameters shadow a global function with the same name");
}

void test_list_hofs_large(void) {
    TEST(eval_str("(set hof-list nil)") != NULL, "initialize large HOF list");
    TEST(eval_str("(set hof-i 0)") != NULL, "initialize large HOF counter");
    TEST(eval_str("(while (< hof-i 1000) (set hof-list (cons hof-i hof-list)) "
                  "(set hof-i (+ hof-i 1)))") != NULL,
         "build large HOF list");

    TEST(gl_value_get_int(eval_str("(len (map 1+ hof-list))")) == 1000, "map on 1000 elements");
    TEST(gl_value_get_int(eval_str("(len (filter even? hof-list))")) == 500,
         "filter on 1000 elements");
    TEST(gl_value_get_int(eval_str("(len (append hof-list '(99)))")) == 1001,
         "append on 1000 elements");
    TEST(gl_value_get_int(eval_str("(len (member 0 hof-list))")) == 1, "member on 1000 elements");
}

void test_gensym(void) {
    TEST(gl_value_get_bool(eval_str("(symbol? (gensym))")) == 1, "gensym returns a symbol");
    TEST(gl_value_get_bool(eval_str("(= (gensym) (gensym))")) == 0, "gensym values are unique");
    TEST(gl_value_get_bool(eval_str("(= (gensym \"x\") (gensym \"x\"))")) == 0,
         "gensym uniqueness does not depend on the prefix");
    TEST(gl_value_get_bool(eval_str("(string-contains? (to-string (gensym \"tmp\")) \"tmp\")")) ==
             1,
         "gensym keeps the requested prefix");
    TEST(gl_value_get_bool(eval_str("(string-starts-with? (to-string (gensym)) \" \")")) == 1,
         "gensym has a hidden leading marker");

    TEST(eval_expr("(gensym 42)") == NULL, "gensym rejects a non-string prefix");

    eval_str("(set gensym-value (gensym))");
    TEST(gl_value_get_bool(eval_str("(= gensym-value (read (to-string gensym-value)))")) == 0,
         "gensym is not readable back to the same symbol");
}

void test_recursion(void) {
    eval_str("(defun endless (n) (+ 1 (endless (+ n 1))))");
    TEST(eval_str("(endless 0)") == NULL, "runaway recursion reports an error");

    eval_str("(defun countdown (n) (if (= n 0) 0 (+ 1 (countdown (- n 1)))))");
    TEST(gl_value_get_int(eval_str("(countdown 100)")) == 100, "bounded recursion works");
    TEST(eval_str("(countdown 1000000)") == NULL, "deep recursion reports an error");

    TEST(eval_str("(+ 1 (assert nil) 3)") == NULL, "argument after a failing one is cleaned up");

    eval_str("(defun count-down (n) (if (= n 0) 0 (count-down (- n 1))))");
    TEST(gl_value_get_int(eval_str("(count-down 5000)")) == 0,
         "tail recursion runs in constant stack");
    TEST(gl_value_get_int(eval_str("(let ((i 0)) (dotimes (k 5000) (set i k)) i)")) == 4999,
         "deep loop with set");
}

int main(void) {
#ifdef GLISP_SOURCE_DIR
    gl_test_chdir(GLISP_SOURCE_DIR);
#endif
    test_derived_envs();
    test_value_copy();
    test_singletons();

    gl_init_global_env();
    gl_load_stdlib();
    test_array_and_vector_eval();
    test_array_vector_predicates();
    test_array_vector_constructors();
    test_array_vector_operations();
    test_array_vector_comparison();
    test_array_vector_gc();
    test_hashmap();
    test_name_suggestions();
    test_std_integer_math();
    test_arithmetic_builtins();
    test_bignum_arithmetic();
    test_comparison_builtins();
    test_sequence_aliases();
    test_math_builtins();
    test_bitwise_builtins();
    test_std_iteration();
    test_std_math_functions();
    test_std_predicates();
    test_std_list_accessors();
    test_std_io_helpers();
    test_logic_forms();
    test_copy();
    test_float_printing();
    test_typeid();
    test_strings();
    test_std_strings();
    test_range();
    test_list_helpers();
    test_long_lists();
    test_alist();
    test_hash_helpers();
    test_sort();
    test_let();
    test_let_star();
    test_cons();
    test_apply();
    test_read_eval();
    test_quasiquote();
    test_macros();
    test_load_relative();
    test_load_failure();
    test_stdlib();
    test_autoload_stdlib();
    test_pragma();
    test_cond();
    test_optional_and_rest();
    test_for();
    test_update_macros();
    test_control_forms();
    test_add_to_list();
    test_defconst();
    test_defvar();
    test_io();
    test_string_escapes();
    test_gc_stress();
    test_call_resolution();
    test_list_hofs_large();
    test_gensym();
    test_recursion();

    gl_destroy_memory_heap();
    return test_status;
}
