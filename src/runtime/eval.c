#include "eval.h"

#include "ast.h"
#include "callstack.h"
#include "diagnostics.h"
#include "location.h"
#include "parser.h"
#include "runtime/env.h"
#include "value.h"

#include <stdio.h>
#include <stdlib.h>

static gl_value_t *__gl_eval_int(gl_ast_node_t *node) {
    return gl_value_make_int(node->value.integral);
}

static gl_value_t *__gl_eval_float(gl_ast_node_t *node) {
    return gl_value_make_float(node->value.floating);
}

static gl_value_t *__gl_eval_symbol(gl_ast_node_t *node) {
    gl_value_t *env = gl_current_env();
    gl_value_t *res = gl_env_get_var((gl_env_t *)env->val, node->value.symbol);

    if (!res) {
        res = gl_env_get_fun((gl_env_t *)env->val, node->value.symbol);
    }

    if (!res) {
        char buf[1024] = "";
        const char *fmt = "undefined variable '%s'";
        snprintf(buf, sizeof(buf), fmt, node->value.symbol);
        gl_diagnostic_report_error(gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, node->location, buf));
        return NULL;
    }
    return res;
}

static gl_value_t *__gl_eval_string(gl_ast_node_t *node) {
    return gl_value_make_string(node->value.string);
}

static gl_value_t *__gl_eval_nil(void) { return gl_value_make_nil(); }

static gl_value_t *__gl_eval_array(gl_ast_node_t *node) {
    size_t size = node->value.array.size;
    gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
    gl_value_array_init(arr, size, NULL);
    for (size_t i = 0; i < size; ++i) {
        arr->data[i] = gl_eval(node->value.array.data[i]);
    }
    return gl_value_make_array_from(arr);
}

static gl_value_t *__gl_eval_vector(gl_ast_node_t *node) {
    gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
    gl_value_vector_init(vec, NULL);
    for (size_t i = 0; i < node->value.vector.size; ++i) {
        gl_value_vector_push_back(vec, gl_eval(*gl_ast_vector_at(&node->value.vector, i)));
    }
    return gl_value_make_vector_from(vec);
}

static void __gl_function_arg_destroy(gl_function_arg_t **arg) {
    if (arg && *arg) {
        free(*arg);
        *arg = NULL;
    }
}

gl_function_args_t *gl_eval_args(gl_ast_node_t *args) {
    gl_function_args_t *res = malloc(sizeof(gl_function_args_t));

    // If have no args, return empty array
    if (args->type == GL_AST_NIL) {
        res->size = 0;
        res->data = NULL;
        return res;
    }

    size_t argc = 0;
    gl_ast_node_t *current = args;

    while (current->type != GL_AST_NIL) {
        current = current->value.cons.cdr;
        ++argc;
    }

    gl_function_args_init(res, argc, gl_function_arg_free);

    current = args;
    for (size_t i = 0; i < argc; ++i, current = current->value.cons.cdr) {
        res->data[i] = malloc(sizeof(gl_function_arg_t));
        res->data[i]->location = current->location;
        res->data[i]->val = gl_eval(current->value.cons.car);
        if (res->data[i]->val == NULL) {
            gl_function_args_destroy(res);
            free(res);
            return NULL;
        }
    }
    return res;
}

static void __gl_define_function_args(const gl_function_t *function, gl_function_args_t *args,
                                      gl_env_t *env) {
    if (!function->params) {
        return;
    }

    size_t arg_idx = 0;
    size_t argc = args ? args->size : 0;

    for (size_t i = 0; i < function->params->size; ++i) {
        gl_function_param_t *p = function->params->data[i];
        if (p->kind == GL_PARAM_REQUIRED) {
            if (arg_idx < argc) {
                gl_env_set_var(env, p->name, gl_value_copy(args->data[arg_idx]->val));
                arg_idx++;
            }
        } else if (p->kind == GL_PARAM_OPTIONAL) {
            if (arg_idx < argc) {
                gl_env_set_var(env, p->name, gl_value_copy(args->data[arg_idx]->val));
                arg_idx++;
            } else {
                if (p->default_val) {
                    gl_value_t *val = gl_eval(p->default_val);
                    gl_env_set_var(env, p->name, val ? gl_value_copy(val) : gl_value_make_nil());
                } else {
                    gl_env_set_var(env, p->name, gl_value_make_nil());
                }
            }
        } else if (p->kind == GL_PARAM_REST) {
            gl_value_t *rest_list = gl_value_make_nil();
            for (size_t j = argc; j > arg_idx; --j) {
                rest_list =
                    gl_value_make_cons_raw(gl_value_copy(args->data[j - 1]->val), rest_list);
            }
            gl_env_set_var(env, p->name, rest_list);
            arg_idx = argc;
        }
    }
}

static gl_value_t *__gl_eval_function_tree(gl_ast_node_t *func_tree, gl_env_t *env) {
    gl_ast_node_t *current = func_tree;
    gl_value_t *res = gl_value_make_nil();
    while (current->type != GL_AST_NIL) {
        res = gl_eval(current->value.cons.car);
        if (!res) {
            return NULL;
        }
        current = current->value.cons.cdr;
    }
    return res;
}

static gl_value_t *__gl_eval_function(gl_function_t *function, gl_function_args_t *args) {
    gl_value_t *env = gl_current_env();
    __gl_define_function_args(function, args, (gl_env_t *)env->val);

    gl_value_t *res = __gl_eval_function_tree(function->tree, (gl_env_t *)env->val);
    if (!res) {
        return NULL;
    }

    return res;
}

static gl_value_t *__gl_eval_list(gl_ast_node_t *node) {
    gl_ast_node_t *func_node = node->value.cons.car;
    gl_ast_node_t *args = node->value.cons.cdr;

    gl_value_t *func_val = NULL;

    gl_value_t *env = gl_current_env();

    // Trying to evaluate expression given instead of function name
    if (func_node->type != GL_AST_SYMBOL) {
        func_val = gl_eval(func_node);
        if (func_val->type != GL_VAL_FUNCTION && func_val->type != GL_VAL_BUILTIN &&
            func_val->type != GL_VAL_SPFORM) {
            gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, func_node->location,
                                                     "Expression result cannot be called"));
            return NULL;
        }
    } else {
        const char *func_name = func_node->value.symbol;
        func_val = gl_env_get_fun((gl_env_t *)env->val, func_name);
        // Evaluating variable value if function is undefined. We trying it because variables can
        // also store functions
        if (func_val == NULL) {
            func_val = gl_env_get_var((gl_env_t *)env->val, func_name);
        }
        // Emmiting error if function still not found
        if (!func_val || !func_val->val) {
            const char *fmt = "undefined function '%s'";
            char buf[1024];
            snprintf(buf, sizeof(buf), fmt, func_name);
            gl_error_t *err = gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, func_node->location, buf);
            gl_diagnostic_report_error(err);
            return NULL;
        }
    }

    // Evaluating function
    switch (func_val->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_val->val;
        gl_function_args_t *argv = gl_eval_args(args);

        if (argv == NULL) {
            return NULL;
        }

        gl_value_t *res = builtin(func_node->location, argv);
        gl_function_args_destroy(argv);
        free(argv);
        return res;
    }
    case GL_VAL_SPFORM: {
        gl_special_form_t spform = func_val->val;

        return spform((gl_env_t *)env->val, args);
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_val->val;
        gl_function_args_t *argv = gl_eval_args(args);

        if (argv == NULL) {
            return NULL;
        }

        if (argv->size < function->min_args ||
            (!function->has_rest && argv->size > function->max_args)) {
            if (function->has_rest) {
                gl_diagnostic_report_variadic_arity_error(func_node->location, function->min_args,
                                                          argv->size);
            } else if (function->min_args == function->max_args) {
                gl_diagnostic_report_arity_error(func_node->location, function->min_args,
                                                 argv->size);
            } else {
                gl_diagnostic_report_range_arity_error(func_node->location, function->min_args,
                                                       function->max_args, argv->size);
            }
            gl_function_args_destroy(argv);
            free(argv);
            return NULL;
        }

        gl_init_call_stack();
        gl_call_stack_push_call(function, func_node);

        gl_value_t *res = __gl_eval_function(function, argv);

        if (!res) {
            gl_call_stack_print_stacktrace();
            gl_function_args_destroy(argv);
            free(argv);
            return NULL;
        }
        gl_call_stack_pop_call();
        gl_function_args_destroy(argv);
        free(argv);
        return res;
    }
    default:
        assert(0 && "Function type must be one of these: GL_VAL_FUNCTION, GL_VAL_BUILTIN, "
                    "GL_VAL_SPFORM");
    }
    return NULL;
}

gl_value_t *gl_call_function(gl_location_t loc, gl_value_t *func_val, gl_function_args_t *argv) {
    if (!func_val) {
        return NULL;
    }
    switch (func_val->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_val->val;
        return builtin(loc, argv);
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_val->val;
        if (argv->size < function->min_args ||
            (!function->has_rest && argv->size > function->max_args)) {
            if (function->has_rest) {
                gl_diagnostic_report_variadic_arity_error(loc, function->min_args, argv->size);
            } else if (function->min_args == function->max_args) {
                gl_diagnostic_report_arity_error(loc, function->min_args, argv->size);
            } else {
                gl_diagnostic_report_range_arity_error(loc, function->min_args, function->max_args,
                                                       argv->size);
            }
            return NULL;
        }
        gl_init_call_stack();
        gl_call_stack_push_call(function, NULL);
        gl_value_t *res = __gl_eval_function(function, argv);
        if (!res) {
            gl_call_stack_print_stacktrace();
            return NULL;
        }
        gl_call_stack_pop_call();
        return res;
    }
    default:
        gl_diagnostic_report_error(
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, loc, "Expression result cannot be called"));
        return NULL;
    }
}

gl_value_t *gl_eval(gl_ast_node_t *node) {
    if (node == NULL)
        return NULL;

    gl_value_t *env = gl_global_env;

    // If got recent calls, using environment of last call
    if (gl_call_stack.size != 0) {
        gl_call_frame_t current_frame = gl_call_stack_current_frame();
        env = current_frame.env;
    }

    switch (node->type) {
    case GL_AST_INT:
        return __gl_eval_int(node);
    case GL_AST_FLOAT:
        return __gl_eval_float(node);
    case GL_AST_SYMBOL:
        return __gl_eval_symbol(node);
    case GL_AST_STRING:
        return __gl_eval_string(node);
    case GL_AST_NIL:
        return __gl_eval_nil();
    case GL_AST_CONS:
        return __gl_eval_list(node);
    case GL_AST_ARRAY:
        return __gl_eval_array(node);
    case GL_AST_VECTOR:
        return __gl_eval_vector(node);
    default:
        return NULL;
    }
}

gl_value_t *gl_parse_and_eval(gl_parser_t *restrict parser, int abort_on_error) {
    gl_ast_node_t *current = gl_parser_parse(parser);

    gl_value_t *res = NULL;

    while (current) {
        res = gl_eval(current);
        // Aborting if got error during parsing
        if (!res && abort_on_error) {
            exit(-1);
        }
        gl_ast_node_t *next = gl_parser_parse(parser);
        gl_ast_destroy(current);
        current = next;
    }

    return res;
}
