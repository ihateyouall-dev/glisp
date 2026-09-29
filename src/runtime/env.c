#include "env.h"

#include "runtime/builtins.h"
#include "runtime/specforms.h"
#include "runtime/value.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t *, gl_value_table)

gl_value_t *gl_global_env = NULL;

gl_env_t *gl_make_env(gl_value_t *parent) {
    gl_env_t *res = malloc(sizeof(gl_env_t));
    res->parent = parent;

    gl_value_table_t *values = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(values, NULL, gl_value_copy);
    res->variables = values;

    gl_value_table_t *functions = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(functions, NULL, gl_value_copy);
    res->functions = functions;

    return res;
}

static void __gl_global_env_define_builtins(gl_env_t *env) {
    gl_env_set_fun(env, "exit", gl_value_make_builtin(gl_builtin_exit));
    gl_env_set_fun(env, "typeof", gl_value_make_builtin(gl_builtin_typeof));
    gl_env_set_fun(env, "print", gl_value_make_builtin(gl_builtin_print));
    gl_env_set_fun(env, "println", gl_value_make_builtin(gl_builtin_println));

    gl_env_set_fun(env, "+", gl_value_make_builtin(gl_builtin_add));
    gl_env_set_fun(env, "-", gl_value_make_builtin(gl_builtin_sub));
    gl_env_set_fun(env, "*", gl_value_make_builtin(gl_builtin_mul));
    gl_env_set_fun(env, "/", gl_value_make_builtin(gl_builtin_div));
    gl_env_set_fun(env, "%", gl_value_make_builtin(gl_builtin_mod));
    gl_env_set_fun(env, "mod", gl_value_make_builtin(gl_builtin_mod));

    gl_env_set_fun(env, "=", gl_value_make_builtin(gl_builtin_eq));
    gl_env_set_fun(env, "eq", gl_value_make_builtin(gl_builtin_eq));
    gl_env_set_fun(env, ">", gl_value_make_builtin(gl_builtin_gt));
    gl_env_set_fun(env, "<", gl_value_make_builtin(gl_builtin_lt));
    gl_env_set_fun(env, ">=", gl_value_make_builtin(gl_builtin_ge));
    gl_env_set_fun(env, "<=", gl_value_make_builtin(gl_builtin_le));

    gl_env_set_fun(env, "car", gl_value_make_builtin(gl_builtin_car));
    gl_env_set_fun(env, "cdr", gl_value_make_builtin(gl_builtin_cdr));
    gl_env_set_fun(env, "cons", gl_value_make_builtin(gl_builtin_cons));

    gl_env_set_fun(env, "int", gl_value_make_builtin(gl_builtin_int_con));
    gl_env_set_fun(env, "float", gl_value_make_builtin(gl_builtin_float_con));
    /* gl_env_set_fun(env, "list", gl_value_make_builtin(gl_builtin_list_con)); */
    gl_env_set_fun(env, "array", gl_value_make_builtin(gl_builtin_array_con));
    gl_env_set_fun(env, "vector", gl_value_make_builtin(gl_builtin_vector_con));

    gl_env_set_fun(env, "len", gl_value_make_builtin(gl_builtin_len));
    gl_env_set_fun(env, "length", gl_value_make_builtin(gl_builtin_len));
    gl_env_set_fun(env, "get", gl_value_make_builtin(gl_builtin_get));
    gl_env_set_fun(env, "nth", gl_value_make_builtin(gl_builtin_get));
    gl_env_set_fun(env, "aref", gl_value_make_builtin(gl_builtin_get));
    gl_env_set_fun(env, "set-at", gl_value_make_builtin(gl_builtin_set_at));
    gl_env_set_fun(env, "aset", gl_value_make_builtin(gl_builtin_set_at));
    gl_env_set_fun(env, "push", gl_value_make_builtin(gl_builtin_vector_push));
    gl_env_set_fun(env, "vector-push", gl_value_make_builtin(gl_builtin_vector_push));
    gl_env_set_fun(env, "pop", gl_value_make_builtin(gl_builtin_vector_pop));
    gl_env_set_fun(env, "vector-pop", gl_value_make_builtin(gl_builtin_vector_pop));

    gl_env_set_fun(env, "apply", gl_value_make_builtin(gl_builtin_apply));
    gl_env_set_fun(env, "load", gl_value_make_builtin(gl_builtin_load));
}

static void __gl_global_env_define_specforms(gl_env_t *env) {
    gl_env_set_fun(env, "if", gl_value_make_specform(&gl_specform_if));
    gl_env_set_fun(env, "defun", gl_value_make_specform(&gl_specform_defun));
    gl_env_set_fun(env, "lambda", gl_value_make_specform(&gl_specform_lambda));
    gl_env_set_fun(env, "set", gl_value_make_specform(&gl_specform_set));
    gl_env_set_fun(env, "set-global", gl_value_make_specform(&gl_specform_set_global));
    gl_env_set_fun(env, "progn", gl_value_make_specform(&gl_specform_progn));
    gl_env_set_fun(env, "while", gl_value_make_specform(&gl_specform_while));
    gl_env_set_fun(env, "quote", gl_value_make_specform(&gl_specform_quote));
    gl_env_set_fun(env, "function", gl_value_make_specform(&gl_specform_function));
    gl_env_set_fun(env, "let", gl_value_make_specform(&gl_specform_let));
    gl_env_set_fun(env, "let*", gl_value_make_specform(&gl_specform_let_star));
    gl_env_set_fun(env, "pragma", gl_value_make_specform(&gl_specform_pragma));
    gl_env_set_fun(env, "cond", gl_value_make_specform(&gl_specform_cond));
    gl_env_set_fun(env, "for", gl_value_make_specform(&gl_specform_for));
    gl_env_set_fun(env, "and", gl_value_make_specform(&gl_specform_and));
    gl_env_set_fun(env, "or", gl_value_make_specform(&gl_specform_or));
}

gl_env_t *gl_make_global_env(void) {
    gl_env_t *res = gl_make_env(NULL);

    gl_env_set_var(res, "t", gl_value_make_symbol("t"));
    gl_env_set_var(res, "else", gl_value_make_symbol("t"));
    gl_env_set_var(res, "nil", gl_value_make_nil());

    // Basic type identifiers
    gl_env_set_var(res, "nil-t", gl_value_make_typeid(GL_VAL_NIL));
    gl_env_set_var(res, "int-t", gl_value_make_typeid(GL_VAL_INT));
    gl_env_set_var(res, "float-t", gl_value_make_typeid(GL_VAL_FLOAT));
    gl_env_set_var(res, "symbol-t", gl_value_make_typeid(GL_VAL_SYMBOL));
    gl_env_set_var(res, "string-t", gl_value_make_typeid(GL_VAL_STRING));
    gl_env_set_var(res, "function-t", gl_value_make_typeid(GL_VAL_FUNCTION));
    gl_env_set_var(res, "list-t", gl_value_make_typeid(GL_VAL_CONS));
    gl_env_set_var(res, "array-t", gl_value_make_typeid(GL_VAL_ARRAY));
    gl_env_set_var(res, "vector-t", gl_value_make_typeid(GL_VAL_VECTOR));
    gl_env_set_var(res, "typeid-t", gl_value_make_typeid(GL_VAL_TYPEID));

    __gl_global_env_define_builtins(res);
    __gl_global_env_define_specforms(res);

    return res;
}

void gl_init_global_env(void) { gl_global_env = gl_value_make_env(gl_make_global_env()); }

void gl_env_destroy(gl_env_t *env) {
    if (!env)
        return;
    gl_value_table_destroy(env->variables);
    free(env->variables);
    gl_value_table_destroy(env->functions);
    free(env->functions);
    free(env);
    env = NULL;
}

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_value_table_insert(env->variables, sym, val);
}

void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val) {
    assert(val->type == GL_VAL_BUILTIN || val->type == GL_VAL_FUNCTION ||
           val->type == GL_VAL_SPFORM);
    gl_value_table_insert(env->functions, sym, val);
}

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym) {
    gl_value_t **obj = gl_value_table_get(env->variables, sym);

    gl_value_t *res = (obj != NULL ? *obj : NULL);

    // Checking for variable in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_var((gl_env_t *)env->parent->val, sym);
    }
    // If have not found in parent environments, return NULL
    if (res == NULL) {
        return NULL;
    }
    return res;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t **obj = gl_value_table_get(env->functions, sym);

    gl_value_t *res = (obj != NULL ? *obj : NULL);

    // Checking for function in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_fun((gl_env_t *)env->parent->val, sym);
    }
    // If have not found in parent environments, return NULL
    if (res == NULL) {
        return NULL;
    }
    return res;
}
