#include "specforms.h"

#include "ast.h"
#include "eval.h"
#include "runtime/env.h"
#include "runtime/value.h"

#ifdef MSC_VER
#include "utils/strdup.h"
#endif

GL_SPECIAL_FORM(if) {
    assert(args);
    assert(args->type == GL_AST_CONS);
    gl_ast_node_t *condition = gl_ast_cons_nth_car(args, 0);
    gl_value_t *condition_val = gl_eval(condition, env);

    // If have no actions, return boolean
    if (args->value.cons.cdr->type == GL_AST_NIL) {
        return gl_value_make_bool(gl_value_get_bool(condition_val));
    }
    gl_ast_node_t *if_true = gl_ast_cons_nth_car(args, 1);
    gl_ast_node_t *if_false = gl_ast_cons_nth_cdr(args, 1);

    if (gl_value_get_bool(condition_val)) {
        return gl_eval(if_true, env);
    } else {
        gl_value_t *res;
        while (if_false->type != GL_AST_NIL) {
            res = gl_eval(gl_ast_cons_nth_car(if_false, 0), env);
            if_false = gl_ast_cons_nth_cdr(if_false, 0);
        }
        return res;
    }
}

GL_SPECIAL_FORM(defun) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *params = gl_ast_cons_nth_car(args, 1);

    gl_function_params_t *func_params = NULL;

    if (params->type != GL_AST_NIL) {
        func_params = malloc(sizeof(gl_function_params_t));
        gl_function_params_init(func_params, gl_ast_cons_length(params), &gl_function_param_free);
        for (size_t i = 0; i < func_params->size; ++i) {
            gl_ast_node_t *param = gl_ast_cons_nth_car(params, i);
            func_params->data[i] = strdup(param->value.symbol);
        }
    }

    gl_ast_node_t *tree = gl_ast_cons_nth_cdr(args, 1);
    gl_value_t *res = gl_value_make_function(gl_ast_copy(tree), func_params, env);

    const char *name = gl_ast_cons_nth_car(args, 0)->value.symbol;
    gl_env_set_fun(env, name, res);

    return res;
}

GL_SPECIAL_FORM(set) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    const char *var_name = gl_ast_cons_nth_car(args, 0)->value.symbol;
    gl_value_t *var_value = gl_eval(gl_ast_cons_nth_car(gl_ast_cons_nth_cdr(args, 0), 0), env);

    gl_env_set_var(env, var_name, var_value);

    return gl_value_make_symbol(var_name);
}

GL_SPECIAL_FORM(progn) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *current = args;
    gl_value_t *res = NULL;

    while (current->type != GL_AST_NIL) {
        res = gl_eval(current->value.cons.car, env);
        current = current->value.cons.cdr;
    }
    return res;
}

GL_SPECIAL_FORM(while) {
    assert(args);
    assert(args->type == GL_AST_CONS);
    gl_ast_node_t *condition = gl_ast_cons_nth_car(args, 0);
    gl_value_t *condition_val = gl_eval(condition, env);

    gl_ast_node_t *body = gl_ast_cons_nth_cdr(args, 0);

    while (gl_value_get_bool(condition_val)) {
        gl_specform_progn(env, body);
        condition_val = gl_eval(condition, env);
    }
    return gl_value_make_nil();
}

GL_SPECIAL_FORM(quote) {
    assert(args);

    assert(args->value.cons.cdr->type == GL_AST_NIL);

    args->value.cons.car->quoted = 1;

    return gl_eval(args->value.cons.car, env);
}
