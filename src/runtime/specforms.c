#include "specforms.h"

#include "ast.h"
#include "callstack.h"
#include "diagnostics.h"
#include "eval.h"
#include "runtime/env.h"
#include "runtime/value.h"

#include <stdio.h>

#ifdef MSC_VER
#include "utils/strdup.h"
#endif

GL_SPECIAL_FORM(if) {
    assert(args);
    assert(args->type == GL_AST_CONS);
    gl_ast_node_t *condition = gl_ast_cons_nth_car(args, 0);
    gl_value_t *condition_val = gl_eval(condition);

    // If have no actions, return boolean
    if (args->value.cons.cdr->type == GL_AST_NIL) {
        return gl_value_make_bool(gl_value_get_bool(condition_val));
    }
    gl_ast_node_t *if_true = gl_ast_cons_nth_car(args, 1);
    gl_ast_node_t *if_false = gl_ast_cons_nth_cdr(args, 1);

    if (gl_value_get_bool(condition_val)) {
        return gl_eval(if_true);
    } else {
        gl_value_t *res;
        while (if_false->type != GL_AST_NIL) {
            res = gl_eval(gl_ast_cons_nth_car(if_false, 0));
            if_false = gl_ast_cons_nth_cdr(if_false, 0);
        }
        return res;
    }
}

GL_SPECIAL_FORM(defun) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *params = gl_ast_cons_nth_car(args, 1);

    gl_function_params_t *func_params = malloc(sizeof(gl_function_params_t));

    if (params->type != GL_AST_NIL) {
        gl_function_params_init(func_params, gl_ast_cons_length(params), &gl_function_param_free);
        for (size_t i = 0; i < func_params->size; ++i) {
            gl_ast_node_t *param = gl_ast_cons_nth_car(params, i);
            func_params->data[i] = strdup(param->value.symbol);
        }
    } else {
        func_params->data = NULL;
        func_params->size = 0;
    }

    gl_ast_node_t *tree = gl_ast_cons_nth_cdr(args, 1);
    const char *name = gl_ast_cons_nth_car(args, 0)->value.symbol;

    gl_value_t *res =
        gl_value_make_function(name, gl_ast_copy(tree), func_params, gl_current_env());
    gl_env_set_fun(env, name, res);

    return res;
}

GL_SPECIAL_FORM(lambda) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *params = gl_ast_cons_nth_car(args, 0);

    gl_function_params_t *func_params = malloc(sizeof(gl_function_params_t));

    if (params->type != GL_AST_NIL) {
        gl_function_params_init(func_params, gl_ast_cons_length(params), &gl_function_param_free);
        for (size_t i = 0; i < func_params->size; ++i) {
            gl_ast_node_t *param = gl_ast_cons_nth_car(params, i);
            func_params->data[i] = strdup(param->value.symbol);
        }
    } else {
        func_params->data = NULL;
        func_params->size = 0;
    }

    gl_ast_node_t *tree = gl_ast_cons_nth_cdr(args, 0);
    gl_value_t *res = gl_value_make_function("anonymous function", gl_ast_copy(tree), func_params,
                                             gl_current_env());

    return res;
}

GL_SPECIAL_FORM(set) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    const char *var_name = gl_ast_cons_nth_car(args, 0)->value.symbol;
    gl_value_t *var_value = gl_eval(gl_ast_cons_nth_car(gl_ast_cons_nth_cdr(args, 0), 0));

    gl_env_set_var(env, var_name, var_value);

    return var_value;
}

GL_SPECIAL_FORM(set_global) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    const char *var_name = gl_ast_cons_nth_car(args, 0)->value.symbol;
    gl_value_t *var_value = gl_eval(gl_ast_cons_nth_car(gl_ast_cons_nth_cdr(args, 0), 0));

    // Getting the global environment
    gl_env_t *global_env = env;

    while (global_env->parent != NULL) {
        global_env = (gl_env_t *)global_env->parent->val;
    }

    gl_env_set_var(global_env, var_name, var_value);

    return var_value;
}

GL_SPECIAL_FORM(progn) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *current = args;
    gl_value_t *res = NULL;

    while (current->type != GL_AST_NIL) {
        res = gl_eval(current->value.cons.car);
        current = current->value.cons.cdr;
    }
    return res;
}

GL_SPECIAL_FORM(while) {
    assert(args);
    assert(args->type == GL_AST_CONS);
    gl_ast_node_t *condition = gl_ast_cons_nth_car(args, 0);
    gl_value_t *condition_val = gl_eval(condition);

    gl_ast_node_t *body = gl_ast_cons_nth_cdr(args, 0);

    while (gl_value_get_bool(condition_val)) {
        gl_specform_progn(env, body);
        condition_val = gl_eval(condition);
    }
    return gl_value_make_nil();
}

GL_SPECIAL_FORM(quote) {
    assert(args);

    assert(args->value.cons.cdr->type == GL_AST_NIL);

    gl_ast_node_t *node = gl_ast_cons_nth_car(args, 0);

    if (node->type == GL_AST_SYMBOL) {
        return gl_value_make_symbol(node->value.symbol);
    }

    if (node->type == GL_AST_CONS) {
        return gl_value_make_cons(node->value.cons);
    }

    return gl_eval(node);
}

GL_SPECIAL_FORM(function) {
    assert(args);

    assert(args->value.cons.cdr->type == GL_AST_NIL);

    gl_ast_node_t *func = gl_ast_cons_nth_car(args, 0);

    if (func->type == GL_AST_SYMBOL) {
        const char *func_name = func->value.symbol;

        gl_value_t *res = gl_env_get_fun(env, func_name);

        if (res == NULL) {
            return gl_env_get_var(env, func_name);
        }
        return res;
    }

    gl_value_t *res = gl_eval(func);

    if (res->type != GL_VAL_FUNCTION && res->type != GL_VAL_BUILTIN && res->type != GL_VAL_SPFORM) {
        return NULL;
    }
    return res;
}

static void __emit_syntax_error(gl_location_t loc, const char *msg) {
    gl_diagnostic_report_error(gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, loc, (char *)msg));
}

static int __gl_parse_binding(gl_ast_node_t *binding, const char **out_name,
                              gl_ast_node_t **out_val_ast) {
    if (binding->type == GL_AST_SYMBOL) {
        *out_name = binding->value.symbol;
        *out_val_ast = NULL;
        return 1;
    }

    if (binding->type == GL_AST_CONS) {
        gl_ast_node_t *var_node = binding->value.cons.car;
        if (var_node->type != GL_AST_SYMBOL) {
            __emit_syntax_error(var_node->location,
                                "Variable name in let binding must be a symbol");
            return 0;
        }
        *out_name = var_node->value.symbol;

        gl_ast_node_t *rest = binding->value.cons.cdr;
        if (rest->type == GL_AST_NIL) {
            *out_val_ast = NULL;
            return 1;
        }

        if (rest->type == GL_AST_CONS) {
            *out_val_ast = rest->value.cons.car;
            if (rest->value.cons.cdr->type != GL_AST_NIL) {
                __emit_syntax_error(rest->value.cons.cdr->location,
                                    "Bad let binding: expected at most one value expression");
                return 0;
            }
            return 1;
        }

        __emit_syntax_error(binding->location, "Malformed let binding");
        return 0;
    }

    __emit_syntax_error(binding->location,
                        "Let binding must be a symbol or a list (symbol [value])");
    return 0;
}

static gl_value_t *__gl_eval_body(gl_ast_node_t *body) {
    gl_value_t *res = gl_value_make_nil();
    gl_ast_node_t *current = body;
    while (current && current->type != GL_AST_NIL) {
        if (current->type != GL_AST_CONS) {
            return gl_eval(current);
        }
        res = gl_eval(current->value.cons.car);
        if (!res) {
            return NULL;
        }
        current = current->value.cons.cdr;
    }
    return res;
}

GL_SPECIAL_FORM(let) {
    if (!args || args->type != GL_AST_CONS) {
        __emit_syntax_error(args ? args->location : (gl_location_t){0},
                            "let requires a bindings list and a body");
        return NULL;
    }

    gl_ast_node_t *bindings_node = args->value.cons.car;
    if (bindings_node->type != GL_AST_CONS && bindings_node->type != GL_AST_NIL) {
        __emit_syntax_error(bindings_node->location,
                            "First argument to let must be a list of bindings");
        return NULL;
    }

    size_t count = 0;
    gl_ast_node_t *curr = bindings_node;
    while (curr->type == GL_AST_CONS) {
        count++;
        curr = curr->value.cons.cdr;
    }

    const char **names = count > 0 ? malloc(sizeof(char *) * count) : NULL;

    gl_value_t *parent_env = gl_current_env();
    gl_env_t *new_env = gl_make_env(parent_env);
    gl_value_t *new_env_val = gl_value_make_env(new_env);

    gl_call_stack_push_env(new_env_val, args, "let");

    curr = bindings_node;
    for (size_t i = 0; i < count; ++i) {
        gl_ast_node_t *binding = curr->value.cons.car;
        const char *name = NULL;
        gl_ast_node_t *val_ast = NULL;

        if (!__gl_parse_binding(binding, &name, &val_ast)) {
            gl_call_stack_pop_env();
            free(names);
            return NULL;
        }

        names[i] = name;
        gl_value_t *val = NULL;
        if (val_ast != NULL) {
            val = gl_eval(val_ast);
            if (!val) {
                gl_call_stack_pop_env();
                free(names);
                return NULL;
            }
        } else {
            val = gl_value_make_nil();
        }

        char temp_key[32];
        snprintf(temp_key, sizeof(temp_key), " %zu", i);
        gl_env_set_var(new_env, temp_key, val);

        curr = curr->value.cons.cdr;
    }

    for (size_t i = 0; i < count; ++i) {
        char temp_key[32];
        snprintf(temp_key, sizeof(temp_key), " %zu", i);
        gl_value_t *val = gl_env_get_var(new_env, temp_key);
        gl_env_set_var(new_env, names[i], val);
    }

    free(names);

    gl_ast_node_t *body = args->value.cons.cdr;
    gl_value_t *res = __gl_eval_body(body);

    gl_call_stack_pop_env();

    return res;
}

GL_SPECIAL_FORM(let_star) {
    if (!args || args->type != GL_AST_CONS) {
        __emit_syntax_error(args ? args->location : (gl_location_t){0},
                            "let* requires a bindings list and a body");
        return NULL;
    }

    gl_ast_node_t *bindings_node = args->value.cons.car;
    if (bindings_node->type != GL_AST_CONS && bindings_node->type != GL_AST_NIL) {
        __emit_syntax_error(bindings_node->location,
                            "First argument to let* must be a list of bindings");
        return NULL;
    }

    gl_value_t *parent_env = gl_current_env();
    gl_env_t *new_env = gl_make_env(parent_env);
    gl_value_t *new_env_val = gl_value_make_env(new_env);

    gl_call_stack_push_env(new_env_val, args, "let*");

    gl_ast_node_t *curr = bindings_node;
    while (curr->type == GL_AST_CONS) {
        gl_ast_node_t *binding = curr->value.cons.car;
        const char *name = NULL;
        gl_ast_node_t *val_ast = NULL;

        if (!__gl_parse_binding(binding, &name, &val_ast)) {
            gl_call_stack_pop_env();
            return NULL;
        }

        gl_value_t *val = NULL;
        if (val_ast != NULL) {
            val = gl_eval(val_ast);
            if (!val) {
                gl_call_stack_pop_env();
                return NULL;
            }
        } else {
            val = gl_value_make_nil();
        }

        gl_env_set_var(new_env, name, val);
        curr = curr->value.cons.cdr;
    }

    gl_ast_node_t *body = args->value.cons.cdr;
    gl_value_t *res = __gl_eval_body(body);

    gl_call_stack_pop_env();

    return res;
}
