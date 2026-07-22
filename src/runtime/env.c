#include "env.h"
#include "ast.h"

#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t, value_table)

gl_env_t *gl_env_create(gl_env_t *parent) {
    gl_env_t *res = malloc(sizeof(gl_env_t));
    res->parent = parent;

    value_table_t *values = malloc(sizeof(value_table_t));
    value_table_init(values);
    res->variables = values;

    value_table_t *functions = malloc(sizeof(value_table_t));
    value_table_init(functions);
    res->functions = functions;

    return res;
}

void gl_env_destroy(gl_env_t *env) {
    if (!env)
        return;
    value_table_destroy(env->variables);
    free(env->variables);
    value_table_destroy(env->functions);
    free(env->functions);
    free(env);
}

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t val) {
    value_table_insert(env->variables, sym, val);
}

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym) {
    gl_value_t *res = value_table_get(env->variables, sym);

    // Checking for variable in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_var(env->parent, sym);
    }
    return res;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t *res = value_table_get(env->functions, sym);

    // Checking for function in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_fun(env->parent, sym);
    }
    return res;
}

GL_BUILTIN(exit) {
    assert(argv);
    assert(argv[0].val);
    exit(*((int *)argv[0].val));
}

static void __gl_eval_function(gl_ast_node_t *root) {}

GL_BUILTIN(funcall) {}
