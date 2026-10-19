#include "specforms.h"

#include "ast.h"
#include "callstack.h"
#include "diagnostics.h"
#include "eval.h"
#include "pragma.h"
#include "runtime/env.h"
#include "runtime/value.h"

#include <stdio.h>
#include <string.h>

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

static void __emit_syntax_error(gl_location_t loc, const char *msg) {
    gl_diagnostic_report_error(gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, loc, (char *)msg));
}

static gl_function_params_t *__gl_parse_function_params(gl_location_t loc, gl_ast_node_t *params) {
    if (!params || params->type == GL_AST_NIL) {
        gl_function_params_t *func_params = malloc(sizeof(gl_function_params_t));
        gl_function_params_init(func_params, 0, &gl_function_param_free);
        return func_params;
    }

    if (params->type != GL_AST_CONS) {
        __emit_syntax_error(params->location, "Function parameters must be a list");
        return NULL;
    }

    size_t capacity = 8;
    size_t count = 0;
    gl_function_param_t **temp = malloc(sizeof(gl_function_param_t *) * capacity);

    typedef enum { PARSE_STATE_REQUIRED, PARSE_STATE_OPTIONAL, PARSE_STATE_REST } parse_state_t;

    parse_state_t state = PARSE_STATE_REQUIRED;
    int saw_rest = 0;

    gl_ast_node_t *curr = params;
    while (curr && curr->type == GL_AST_CONS) {
        gl_ast_node_t *item = curr->value.cons.car;

        if (item->type == GL_AST_SYMBOL && strcmp(item->value.symbol, "&optional") == 0) {
            if (state == PARSE_STATE_OPTIONAL) {
                __emit_syntax_error(item->location, "Duplicate &optional in parameter list");
                goto error;
            }
            if (saw_rest) {
                __emit_syntax_error(item->location, "&optional cannot appear after &rest");
                goto error;
            }
            state = PARSE_STATE_OPTIONAL;
            curr = curr->value.cons.cdr;
            continue;
        }

        if (item->type == GL_AST_SYMBOL && strcmp(item->value.symbol, "&rest") == 0) {
            if (saw_rest) {
                __emit_syntax_error(item->location, "Duplicate &rest in parameter list");
                goto error;
            }
            state = PARSE_STATE_REST;
            saw_rest = 1;
            curr = curr->value.cons.cdr;
            continue;
        }

        if (state == PARSE_STATE_REQUIRED) {
            if (item->type != GL_AST_SYMBOL) {
                __emit_syntax_error(item->location, "Required parameter name must be a symbol");
                goto error;
            }
            if (count == capacity) {
                capacity *= 2;
                temp = realloc(temp, sizeof(gl_function_param_t *) * capacity);
            }
            temp[count++] = gl_function_param_make(item->value.symbol, GL_PARAM_REQUIRED, NULL);
        } else if (state == PARSE_STATE_OPTIONAL) {
            if (item->type == GL_AST_SYMBOL) {
                if (count == capacity) {
                    capacity *= 2;
                    temp = realloc(temp, sizeof(gl_function_param_t *) * capacity);
                }
                temp[count++] = gl_function_param_make(item->value.symbol, GL_PARAM_OPTIONAL, NULL);
            } else if (item->type == GL_AST_CONS) {
                gl_ast_node_t *name_node = item->value.cons.car;
                if (name_node->type != GL_AST_SYMBOL) {
                    __emit_syntax_error(name_node->location,
                                        "Optional parameter name must be a symbol");
                    goto error;
                }
                gl_ast_node_t *rest_spec = item->value.cons.cdr;
                gl_ast_node_t *def_ast = NULL;
                if (rest_spec->type == GL_AST_CONS) {
                    def_ast = gl_ast_copy(rest_spec->value.cons.car);
                    if (rest_spec->value.cons.cdr->type != GL_AST_NIL) {
                        __emit_syntax_error(item->location,
                                            "Bad &optional parameter: expected (name [default])");
                        if (def_ast) {
                            gl_ast_destroy(def_ast);
                        }
                        goto error;
                    }
                } else if (rest_spec->type != GL_AST_NIL) {
                    __emit_syntax_error(item->location, "Bad &optional parameter specification");
                    goto error;
                }
                if (count == capacity) {
                    capacity *= 2;
                    temp = realloc(temp, sizeof(gl_function_param_t *) * capacity);
                }
                temp[count++] =
                    gl_function_param_make(name_node->value.symbol, GL_PARAM_OPTIONAL, def_ast);
            } else {
                __emit_syntax_error(item->location,
                                    "Optional parameter must be a symbol or (symbol [default])");
                goto error;
            }
        } else if (state == PARSE_STATE_REST) {
            if (item->type != GL_AST_SYMBOL) {
                __emit_syntax_error(item->location, "&rest parameter name must be a symbol");
                goto error;
            }
            if (curr->value.cons.cdr->type != GL_AST_NIL) {
                __emit_syntax_error(curr->value.cons.cdr->location,
                                    "Only a single variable name allowed after &rest");
                goto error;
            }
            if (count == capacity) {
                capacity *= 2;
                temp = realloc(temp, sizeof(gl_function_param_t *) * capacity);
            }
            temp[count++] = gl_function_param_make(item->value.symbol, GL_PARAM_REST, NULL);
        }

        curr = curr->value.cons.cdr;
    }

    if (curr && curr->type != GL_AST_NIL) {
        __emit_syntax_error(curr->location, "Malformed parameter list");
        goto error;
    }

    if (saw_rest && (count == 0 || temp[count - 1]->kind != GL_PARAM_REST)) {
        __emit_syntax_error(loc, "&rest requires a parameter name");
        goto error;
    }

    gl_function_params_t *func_params = malloc(sizeof(gl_function_params_t));
    gl_function_params_init(func_params, count, &gl_function_param_free);
    for (size_t i = 0; i < count; ++i) {
        func_params->data[i] = temp[i];
    }
    free(temp);
    return func_params;

error:
    for (size_t i = 0; i < count; ++i) {
        gl_function_param_free(&temp[i]);
    }
    free(temp);
    return NULL;
}

GL_SPECIAL_FORM(defun) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *name_node = gl_ast_cons_nth_car(args, 0);
    if (!name_node || name_node->type != GL_AST_SYMBOL) {
        __emit_syntax_error(name_node ? name_node->location : args->location,
                            "Function name must be a symbol");
        return NULL;
    }
    const char *name = name_node->value.symbol;

    gl_ast_node_t *params = gl_ast_cons_nth_car(args, 1);
    gl_function_params_t *func_params = __gl_parse_function_params(args->location, params);
    if (params->type != GL_AST_NIL && !func_params) {
        return NULL;
    }

    gl_ast_node_t *tree = gl_ast_cons_nth_cdr(args, 1);
    gl_value_t *res =
        gl_value_make_function(name, gl_ast_copy(tree), func_params, gl_current_env());
    gl_env_set_fun(env, name, res);

    return res;
}

GL_SPECIAL_FORM(lambda) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *params = gl_ast_cons_nth_car(args, 0);
    gl_function_params_t *func_params = __gl_parse_function_params(args->location, params);
    if (params->type != GL_AST_NIL && !func_params) {
        return NULL;
    }

    gl_ast_node_t *tree = gl_ast_cons_nth_cdr(args, 0);
    gl_value_t *res = gl_value_make_function("anonymous function", gl_ast_copy(tree), func_params,
                                             gl_current_env());

    return res;
}

GL_SPECIAL_FORM(set) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *name_node = gl_ast_cons_nth_car(args, 0);
    if (!name_node || name_node->type != GL_AST_SYMBOL) {
        __emit_syntax_error(name_node ? name_node->location : args->location,
                            "Variable name in set must be a symbol");
        return NULL;
    }
    const char *var_name = name_node->value.symbol;

    if (gl_env_is_const(env, var_name)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot modify constant '%s'", var_name);
        __emit_syntax_error(name_node->location, buf);
        return NULL;
    }

    gl_value_t *var_value = gl_eval(gl_ast_cons_nth_car(gl_ast_cons_nth_cdr(args, 0), 0));

    // Find the nearest environment where var_name is bound
    gl_env_t *target_env = env;
    while (target_env != NULL) {
        if (gl_value_table_get(target_env->variables, var_name) != NULL) {
            break;
        }
        target_env = target_env->parent ? (gl_env_t *)target_env->parent->val : NULL;
    }
    if (target_env == NULL) {
        target_env = env;
    }

    gl_env_set_var(target_env, var_name, var_value);

    return var_value;
}

GL_SPECIAL_FORM(set_global) {
    assert(args);
    assert(args->type == GL_AST_CONS);

    gl_ast_node_t *name_node = gl_ast_cons_nth_car(args, 0);
    if (!name_node || name_node->type != GL_AST_SYMBOL) {
        __emit_syntax_error(name_node ? name_node->location : args->location,
                            "Variable name in set-global must be a symbol");
        return NULL;
    }
    const char *var_name = name_node->value.symbol;

    if (gl_env_is_const(env, var_name)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot modify constant '%s'", var_name);
        __emit_syntax_error(name_node->location, buf);
        return NULL;
    }

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

        if (gl_env_is_const(env, name)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cannot bind constant '%s'", name);
            __emit_syntax_error(binding->location, buf);
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

        if (gl_env_is_const(env, name)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cannot bind constant '%s'", name);
            __emit_syntax_error(binding->location, buf);
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

GL_SPECIAL_FORM(pragma) {
    assert(args);
    if (args->type == GL_AST_NIL) {
        gl_diagnostic_report_variadic_arity_error(args->location, 1, 0);
        return NULL;
    }

    gl_ast_node_t *name_node = gl_ast_cons_nth_car(args, 0);
    if (!name_node || name_node->type != GL_AST_SYMBOL) {
        gl_location_t loc = name_node ? name_node->location : args->location;
        gl_error_t *err =
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, loc, "Pragma directive name must be a symbol");
        gl_diagnostic_report_error(err);
        return NULL;
    }

    const char *pragma_name = name_node->value.symbol;
    gl_ast_node_t *rest_ast = gl_ast_cons_nth_cdr(args, 0);

    gl_function_args_t *pragma_args = gl_eval_args(rest_ast);
    if (!pragma_args) {
        return NULL;
    }

    gl_value_t *res = gl_dispatch_pragma(name_node->location, pragma_name, pragma_args);
    gl_function_args_destroy(pragma_args);
    free(pragma_args);
    return res;
}

GL_SPECIAL_FORM(cond) {
    if (!args || args->type == GL_AST_NIL) {
        return gl_value_make_nil();
    }
    if (args->type != GL_AST_CONS) {
        __emit_syntax_error(args->location, "Malformed cond form: expected a list of clauses");
        return NULL;
    }

    gl_ast_node_t *clause_node = args;
    while (clause_node && clause_node->type == GL_AST_CONS) {
        gl_ast_node_t *clause = clause_node->value.cons.car;
        if (!clause || clause->type != GL_AST_CONS) {
            __emit_syntax_error(clause ? clause->location : args->location,
                                "cond clause must be a non-empty list");
            return NULL;
        }

        gl_ast_node_t *test_node = clause->value.cons.car;
        int is_else =
            (test_node->type == GL_AST_SYMBOL && strcmp(test_node->value.symbol, "else") == 0);

        gl_value_t *test_val = NULL;
        if (is_else) {
            test_val = gl_value_make_bool(1);
        } else {
            test_val = gl_eval(test_node);
            if (!test_val) {
                return NULL;
            }
        }

        if (gl_value_get_bool(test_val)) {
            gl_ast_node_t *body = clause->value.cons.cdr;
            if (body->type == GL_AST_NIL) {
                return test_val;
            }
            return __gl_eval_body(body);
        }

        clause_node = clause_node->value.cons.cdr;
    }

    if (clause_node && clause_node->type != GL_AST_NIL) {
        __emit_syntax_error(clause_node->location, "Malformed cond clauses list");
        return NULL;
    }

    return gl_value_make_nil();
}

GL_SPECIAL_FORM(for) {
    if (!args || args->type == GL_AST_NIL) {
        return gl_value_make_nil();
    }
    if (args->type != GL_AST_CONS) {
        __emit_syntax_error(args->location,
                            "Malformed for form: expected list of variable name and collection");
        return NULL;
    }

    gl_ast_node_t *iter_node = args->value.cons.car;

    if (iter_node->type != GL_AST_CONS) {
        __emit_syntax_error(iter_node->location,
                            "Malformed for form: expected list of variable name and collection");
        return NULL;
    }

    gl_ast_node_t *iter_name = iter_node->value.cons.car;
    if (iter_name->type != GL_AST_SYMBOL) {
        __emit_syntax_error(iter_name->location, "Variable name in for form must be a symbol");
        return NULL;
    }

    if (gl_env_is_const(env, iter_name->value.symbol)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot bind constant '%s'", iter_name->value.symbol);
        __emit_syntax_error(iter_name->location, buf);
        return NULL;
    }

    gl_ast_node_t *iter_coll_cons = iter_node->value.cons.cdr;
    if (iter_coll_cons->type != GL_AST_CONS) {
        __emit_syntax_error(iter_node->location,
                            "Malformed for form: expected list of variable name and collection");
        return NULL;
    }

    if (iter_coll_cons->value.cons.cdr->type != GL_AST_NIL) {
        __emit_syntax_error(iter_node->location,
                            "Bad for form: expected exactly (variable collection)");
        return NULL;
    }

    gl_ast_node_t *iter_coll_node = iter_coll_cons->value.cons.car;
    gl_value_t *iter_coll = gl_eval(iter_coll_node);
    if (!iter_coll) {
        return NULL;
    }

    const char *var_name = iter_name->value.symbol;
    gl_ast_node_t *body = args->value.cons.cdr;

    gl_value_t *parent_env = gl_current_env();
    gl_env_t *new_env = gl_make_env(parent_env);
    gl_value_t *new_env_val = gl_value_make_env(new_env);

    gl_call_stack_push_env(new_env_val, iter_node, "for");

    if (iter_coll->type == GL_VAL_NIL) {
        gl_call_stack_pop_env();
        return gl_value_make_nil();
    } else if (iter_coll->type == GL_VAL_CONS) {
        gl_value_t *curr = iter_coll;
        while (curr && curr->type == GL_VAL_CONS) {
            gl_value_cons_t *cons = curr->val;
            gl_env_set_var(new_env, var_name, gl_value_copy(cons->car));
            if (!__gl_eval_body(body)) {
                gl_call_stack_pop_env();
                return NULL;
            }
            curr = cons->cdr;
        }
        if (curr && curr->type != GL_VAL_NIL) {
            gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, iter_coll_node->location,
                                            "Cannot iterate over improper list");
            gl_diagnostic_report_error(err);
            gl_call_stack_pop_env();
            return NULL;
        }
    } else if (iter_coll->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = iter_coll->val;
        for (size_t i = 0; i < arr->size; ++i) {
            gl_env_set_var(new_env, var_name, gl_value_copy(arr->data[i]));
            if (!__gl_eval_body(body)) {
                gl_call_stack_pop_env();
                return NULL;
            }
        }
    } else if (iter_coll->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = iter_coll->val;
        for (size_t i = 0; i < vec->size; ++i) {
            gl_env_set_var(new_env, var_name, gl_value_copy(*gl_value_vector_at(vec, i)));
            if (!__gl_eval_body(body)) {
                gl_call_stack_pop_env();
                return NULL;
            }
        }
    } else if (iter_coll->type == GL_VAL_STRING) {
        const char *s = iter_coll->val;
        size_t slen = strlen(s);
        char ch_buf[2] = {0, 0};
        for (size_t i = 0; i < slen; ++i) {
            ch_buf[0] = s[i];
            gl_env_set_var(new_env, var_name, gl_value_make_string(ch_buf));
            if (!__gl_eval_body(body)) {
                gl_call_stack_pop_env();
                return NULL;
            }
        }
    } else {
        gl_error_t *err =
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, iter_coll_node->location,
                          "Expected a collection (list, array, vector, or string) in for form");
        gl_diagnostic_report_error(err);
        gl_call_stack_pop_env();
        return NULL;
    }

    gl_call_stack_pop_env();
    return gl_value_make_nil();
}

GL_SPECIAL_FORM(and) {
    if (!args || args->type == GL_AST_NIL) {
        return gl_value_make_nil();
    }

    gl_ast_node_t *current = args;
    while (current->type != GL_AST_NIL) {
        gl_value_t *current_val = gl_eval(current->value.cons.car);

        if (!gl_value_get_bool(current_val)) {
            return gl_value_make_bool(0);
        }
        current = current->value.cons.cdr;
    }
    return gl_value_make_bool(1);
}

GL_SPECIAL_FORM(or) {
    if (!args || args->type == GL_AST_NIL) {
        return gl_value_make_nil();
    }

    gl_ast_node_t *current = args;
    while (current->type != GL_AST_NIL) {
        gl_value_t *current_val = gl_eval(current->value.cons.car);

        if (gl_value_get_bool(current_val)) {
            return gl_value_make_bool(1);
        }
        current = current->value.cons.cdr;
    }
    return gl_value_make_bool(0);
}

GL_SPECIAL_FORM(add_to_list) {
    if (!args || args->type != GL_AST_CONS) {
        __emit_syntax_error(args ? args->location : (gl_location_t){0},
                            "add-to-list requires at least 2 arguments: (add-to-list var element "
                            "&optional append)");
        return NULL;
    }

    gl_ast_node_t *var_node = args->value.cons.car;
    const char *var_name = NULL;

    if (var_node->type == GL_AST_SYMBOL) {
        var_name = var_node->value.symbol;
    } else if (var_node->type == GL_AST_CONS && var_node->value.cons.car &&
               var_node->value.cons.car->type == GL_AST_SYMBOL &&
               strcmp(var_node->value.cons.car->value.symbol, "quote") == 0 &&
               var_node->value.cons.cdr && var_node->value.cons.cdr->type == GL_AST_CONS &&
               var_node->value.cons.cdr->value.cons.car &&
               var_node->value.cons.cdr->value.cons.car->type == GL_AST_SYMBOL) {
        var_name = var_node->value.cons.cdr->value.cons.car->value.symbol;
    } else {
        __emit_syntax_error(
            var_node->location,
            "First argument to add-to-list must be a variable symbol or quoted symbol");
        return NULL;
    }

    if (gl_env_is_const(env, var_name)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot modify constant '%s'", var_name);
        __emit_syntax_error(var_node->location, buf);
        return NULL;
    }

    gl_ast_node_t *rest1 = args->value.cons.cdr;
    if (!rest1 || rest1->type != GL_AST_CONS) {
        __emit_syntax_error(args->location, "add-to-list requires at least 2 arguments: "
                                            "(add-to-list var element &optional append)");
        return NULL;
    }

    gl_ast_node_t *elem_node = rest1->value.cons.car;
    gl_ast_node_t *rest2 = rest1->value.cons.cdr;
    gl_ast_node_t *app_node = NULL;

    if (rest2 && rest2->type == GL_AST_CONS) {
        app_node = rest2->value.cons.car;
        if (rest2->value.cons.cdr && rest2->value.cons.cdr->type != GL_AST_NIL) {
            __emit_syntax_error(args->location, "add-to-list accepts at most 3 arguments");
            return NULL;
        }
    } else if (rest2 && rest2->type != GL_AST_NIL) {
        __emit_syntax_error(args->location, "Malformed add-to-list form");
        return NULL;
    }

    gl_value_t *elem = gl_eval(elem_node);
    if (!elem) {
        return NULL;
    }

    int do_append = 0;
    if (app_node) {
        gl_value_t *app_val = gl_eval(app_node);
        if (!app_val) {
            return NULL;
        }
        do_append = gl_value_get_bool(app_val);
    }

    // Find the nearest environment where var_name is bound
    gl_env_t *target_env = env;
    while (target_env != NULL) {
        if (gl_value_table_get(target_env->variables, var_name) != NULL) {
            break;
        }
        target_env = target_env->parent ? (gl_env_t *)target_env->parent->val : NULL;
    }
    if (target_env == NULL) {
        target_env = env;
    }

    gl_value_t *cur_val = gl_env_get_var(env, var_name);
    if (cur_val && cur_val->type != GL_VAL_NIL && cur_val->type != GL_VAL_CONS) {
        gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, var_node->location,
                                        "Variable in add-to-list must contain a list or nil");
        gl_diagnostic_report_error(err);
        return NULL;
    }

    // Check if element is already present in the list
    gl_value_t *p = cur_val;
    while (p && p->type == GL_VAL_CONS) {
        gl_value_cons_t *cons = p->val;
        if (gl_value_compare(cons->car, elem) == 0) {
            return cur_val ? cur_val : gl_value_make_nil();
        }
        p = cons->cdr;
    }

    gl_value_t *new_list = NULL;
    if (!do_append) {
        gl_value_t *tail =
            (cur_val && cur_val->type == GL_VAL_CONS) ? cur_val : gl_value_make_nil();
        new_list = gl_value_make_cons_raw(elem, tail);
    } else {
        if (!cur_val || cur_val->type == GL_VAL_NIL) {
            new_list = gl_value_make_cons_raw(elem, gl_value_make_nil());
        } else {
            gl_value_t *head = NULL;
            gl_value_t *tail = NULL;
            gl_value_t *curr = cur_val;
            while (curr && curr->type == GL_VAL_CONS) {
                gl_value_cons_t *cons = curr->val;
                gl_value_t *cell = gl_value_make_cons_raw(cons->car, gl_value_make_nil());
                if (!head) {
                    head = cell;
                    tail = cell;
                } else {
                    gl_value_cons_t *tail_cons = tail->val;
                    tail_cons->cdr = cell;
                    tail = cell;
                }
                curr = cons->cdr;
            }
            gl_value_t *elem_cell = gl_value_make_cons_raw(elem, gl_value_make_nil());
            if (!head) {
                head = elem_cell;
            } else {
                gl_value_cons_t *tail_cons = tail->val;
                tail_cons->cdr = elem_cell;
            }
            new_list = head;
        }
    }

    gl_env_set_var(target_env, var_name, new_list);
    return new_list;
}

GL_SPECIAL_FORM(defconst) {
    if (!args || args->type != GL_AST_CONS) {
        __emit_syntax_error(args ? args->location : (gl_location_t){0},
                            "defconst requires a constant name and value: (defconst name value "
                            "[&optional docstring])");
        return NULL;
    }

    gl_ast_node_t *name_node = gl_ast_cons_nth_car(args, 0);
    if (!name_node || name_node->type != GL_AST_SYMBOL) {
        __emit_syntax_error(name_node ? name_node->location : args->location,
                            "Constant name must be a symbol");
        return NULL;
    }
    const char *name = name_node->value.symbol;

    if (strcmp(name, "t") == 0 || strcmp(name, "nil") == 0 || strcmp(name, "else") == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot re-define built-in constant '%s'", name);
        __emit_syntax_error(name_node->location, buf);
        return NULL;
    }

    gl_ast_node_t *rest1 = args->value.cons.cdr;
    if (!rest1 || rest1->type != GL_AST_CONS) {
        __emit_syntax_error(
            args->location,
            "defconst requires a value: (defconst name value [&optional docstring])");
        return NULL;
    }

    gl_ast_node_t *val_node = rest1->value.cons.car;
    gl_ast_node_t *rest2 = rest1->value.cons.cdr;
    if (rest2 && rest2->type == GL_AST_CONS) {
        gl_ast_node_t *doc_node = rest2->value.cons.car;
        if (doc_node->type != GL_AST_STRING) {
            __emit_syntax_error(doc_node->location, "Docstring must be a string");
            return NULL;
        }
        if (rest2->value.cons.cdr && rest2->value.cons.cdr->type != GL_AST_NIL) {
            __emit_syntax_error(args->location, "Too many arguments to defconst");
            return NULL;
        }
    } else if (rest2 && rest2->type != GL_AST_NIL) {
        __emit_syntax_error(args->location, "Malformed defconst form");
        return NULL;
    }

    gl_value_t *val = gl_eval(val_node);
    if (!val) {
        return NULL;
    }

    if (gl_env_is_const(env, name)) {
        gl_value_t *existing = gl_env_get_var(env, name);
        if (existing && gl_value_compare(existing, val) == 0) {
            return val;
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "Cannot re-define constant '%s'", name);
        __emit_syntax_error(name_node->location, buf);
        return NULL;
    }

    gl_env_t *global_env = env;
    while (global_env->parent != NULL) {
        global_env = (gl_env_t *)global_env->parent->val;
    }

    gl_env_set_const(global_env, name, val);
    return val;
}
