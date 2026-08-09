#include "env.h"
#include "runtime/builtins.h"
#include "runtime/specforms.h"
#include "runtime/value.h"
#include "utils/vector.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t *, gl_value_table)

gl_env_t *gl_make_env(gl_env_t *parent) {
    gl_env_t *res = malloc(sizeof(gl_env_t));
    res->parent = parent;

    gl_value_table_t *values = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(values, gl_value_destroy, gl_value_copy);
    res->variables = values;

    gl_value_table_t *functions = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(functions, gl_value_destroy, gl_value_copy);
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
    gl_env_set_fun(env, "!", gl_value_make_builtin(gl_builtin_not));
    gl_env_set_fun(env, "not", gl_value_make_builtin(gl_builtin_not));
    gl_env_set_fun(env, "&", gl_value_make_builtin(gl_builtin_and));
    gl_env_set_fun(env, "and", gl_value_make_builtin(gl_builtin_and));
    gl_env_set_fun(env, "|", gl_value_make_builtin(gl_builtin_or));
    gl_env_set_fun(env, "or", gl_value_make_builtin(gl_builtin_or));

    gl_env_set_fun(env, "car", gl_value_make_builtin(gl_builtin_car));
    gl_env_set_fun(env, "cdr", gl_value_make_builtin(gl_builtin_cdr));
}

static void __gl_global_env_define_specforms(gl_env_t *env) {
    gl_env_set_fun(env, "if", gl_value_make_specform(&gl_specform_if));
    gl_env_set_fun(env, "defun", gl_value_make_specform(&gl_specform_defun));
    gl_env_set_fun(env, "lambda", gl_value_make_specform(&gl_specform_lambda));
    gl_env_set_fun(env, "set", gl_value_make_specform(&gl_specform_set));
    gl_env_set_fun(env, "progn", gl_value_make_specform(&gl_specform_progn));
    gl_env_set_fun(env, "while", gl_value_make_specform(&gl_specform_while));
    gl_env_set_fun(env, "quote", gl_value_make_specform(&gl_specform_quote));
}

gl_env_t *gl_make_global_env() {
    gl_env_t *res = gl_make_env(NULL);

    gl_env_set_var(res, "t", gl_value_make_symbol("t"));
    gl_env_set_var(res, "nil", gl_value_make_nil());

    __gl_global_env_define_builtins(res);
    __gl_global_env_define_specforms(res);

    return res;
}

void gl_env_destroy(gl_env_t *env) {
    if (!env)
        return;
    gl_value_table_destroy(env->variables);
    free(env->variables);
    gl_value_table_destroy(env->functions);
    free(env->functions);
    free(env);
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
    gl_value_t **res = gl_value_table_get(env->variables, sym);

    // Checking for variable in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_var(env->parent, sym);
    }
    return *res;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t **res = gl_value_table_get(env->functions, sym);

    // Checking for function in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_fun(env->parent, sym);
    }
    return *res;
}
