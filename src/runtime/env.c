#include "env.h"
#include "ast.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t *, gl_value_table)

gl_env_t *gl_env_create(gl_env_t *parent) {
    gl_env_t *res = malloc(sizeof(gl_env_t));
    res->parent = parent;

    gl_value_table_t *values = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(values, &gl_value_destroy);
    res->variables = values;

    gl_value_table_t *functions = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(functions, &gl_value_destroy);
    res->functions = functions;

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
    assert(val->type == GL_VAL_BUILTIN || val->type == GL_VAL_FUNCTION);
    gl_value_table_insert(env->functions, sym, val);
}

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym) {
    gl_value_t *res = *gl_value_table_get(env->variables, sym);

    // Checking for variable in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_var(env->parent, sym);
    }
    return res;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t *res = *gl_value_table_get(env->functions, sym);

    // Checking for function in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_fun(env->parent, sym);
    }
    return res;
}

gl_value_t *gl_value_make_int(int64_t num) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    res->val = malloc(sizeof(int64_t));
    *(int64_t *)res->val = num;
    return res;
}

gl_value_t *gl_value_make_float(long double num) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FLOAT;
    res->val = malloc(sizeof(long double));
    *(long double *)res->val = num;
    return res;
}

gl_value_t *gl_value_make_symbol(const char *sym) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_SYMBOL;
    res->val = strdup(sym);
    return res;
}

// Create literal cons of values
gl_value_t *gl_value_make_cons(gl_ast_cons_t cons) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = malloc(sizeof(gl_value_cons_t));
    // Constructing car
    gl_value_t *car;
    switch (cons.car->type) {
    case GL_AST_NIL:
        car = gl_value_make_nil();
        break;
    case GL_AST_INT:
        car = gl_value_make_int(cons.car->value.integral);
        break;
    case GL_AST_FLOAT:
        car = gl_value_make_float(cons.car->value.floating);
        break;
    case GL_AST_SYMBOL:
        car = gl_value_make_symbol(cons.car->value.symbol);
        break;
    case GL_AST_CONS:
        car = gl_value_make_cons(cons.car->value.cons);
        break;
    }
    (*(gl_value_cons_t *)res->val).car = car;

    if (cons.cdr->type == GL_AST_NIL) {
        (*(gl_value_cons_t *)res->val).cdr = gl_value_make_nil();
    } else if (cons.cdr->type == GL_AST_CONS) {
        (*(gl_value_cons_t *)res->val).cdr = gl_value_make_cons(cons.cdr->value.cons);
    } else {
        assert(0 && "CDR of CONS can be only other CONS or NIL");
    }

    return res;
}

gl_value_t *gl_value_make_nil(void) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_NIL;
    res->val = NULL;
    return res;
}

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_BUILTIN;
    res->val = malloc(sizeof(gl_builtin_t));
    *(gl_builtin_t *)res->val = builtin;
    return res;
}

gl_value_t *gl_value_make_function(gl_ast_node_t *tree, char **args, gl_env_t *closure) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FUNCTION;
    gl_function_t *func = malloc(sizeof(gl_function_t));
    func->tree = tree;
    func->args = args;
    func->closure = closure;
    res->val = func;
    return res;
}

void gl_value_destroy(gl_value_t **val) {
    if (val == NULL || *val == NULL)
        return;

    switch ((*val)->type) {
    case GL_VAL_NIL:
        break;
    case GL_VAL_INT:
    case GL_VAL_FLOAT:
    case GL_VAL_SYMBOL:
    case GL_VAL_FUNCTION:
    case GL_VAL_BUILTIN:
        free((*val)->val);
        break;
    case GL_VAL_CONS:
        gl_value_destroy(&((gl_value_cons_t *)((*val)->val))->car);
        gl_value_destroy(&((gl_value_cons_t *)((*val)->val))->cdr);
        free((*val)->val);
    default:
        break;
    }
    free(*val);
    *val = NULL;
}

GL_BUILTIN(exit) {
    assert(argv);
    assert(argv[0].val);
    exit(*((int *)argv[0].val));
}

GL_BUILTIN(add) {
    assert(argv);
    gl_value_t res;
    res.type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    for (size_t i = 0; i < argc; ++i) {
        gl_value_t *current = argv + i;
        assert(current->type == GL_VAL_INT || current->type == GL_VAL_FLOAT);
        if (res.type == GL_VAL_INT) {
            if (current->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res.type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum += *(int64_t *)current->val;
            }
        }
        if (res.type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum += (long double)*(int64_t *)current->val;
            } else {
                fsum += *(long double *)current->val;
            }
        }
    }
    // Finally assigning sum to result
    if (res.type == GL_VAL_INT) {
        res.val = malloc(sizeof(int64_t));
        *(int64_t *)res.val = isum;
    } else {
        res.val = malloc(sizeof(long double));
        *(long double *)res.val = fsum;
    }
    return res;
}
