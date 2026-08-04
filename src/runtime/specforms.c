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
    gl_ast_node_t *if_true = gl_ast_cons_nth_car(args, 1);
    gl_ast_node_t *if_false = gl_ast_cons_nth_cdr(args, 1);
    gl_value_t *condition_val = gl_eval(condition, env);

    // Everything except NIL is interpreted as true
    if (condition_val->type != GL_VAL_NIL) {
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
