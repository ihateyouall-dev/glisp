#include "eval.h"
#include "ast.h"
#include "diagnostics.h"
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

static gl_value_t *__gl_eval_symbol(gl_ast_node_t *node, gl_env_t *env) {
    gl_value_t *res = gl_env_get_var(env, node->value.symbol);
    if (!res) {
        char buf[1024] = "";
        const char *fmt = "undefined variable '%s'";
        snprintf(buf, sizeof(buf), fmt, node->value.symbol);
        gl_diagnostic_report_error(gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, node->location, buf));
        return NULL;
    }
    return res;
}

static gl_value_t *__gl_eval_nil(void) { return gl_value_make_nil(); }

static void __gl_function_arg_destroy(gl_function_arg_t **arg) {
    gl_value_destroy(&(*arg)->val);
    free(*arg);
}

static gl_function_args_t *__gl_eval_args(gl_ast_node_t *args, gl_env_t *env) {
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

    gl_function_args_init(res, argc, &__gl_function_arg_destroy);

    current = args;
    for (size_t i = 0; i < argc; ++i, current = current->value.cons.cdr) {
        res->data[i] = malloc(sizeof(gl_function_arg_t));
        res->data[i]->location = current->location;
        res->data[i]->val = gl_eval(current->value.cons.car, env);
        if (res->data[i]->val == NULL) {
            return NULL;
        }
    }
    return res;
}

static void __gl_define_function_args(const gl_function_t *function, gl_function_args_t *args,
                                      gl_env_t *env) {
    for (size_t i = 0; i < function->params->size; ++i) {
        gl_env_set_var(env, function->params->data[i], gl_value_copy(args->data[i]->val));
    }
}

static gl_value_t *__gl_eval_function_tree(gl_ast_node_t *func_tree, gl_env_t *env) {
    gl_ast_node_t *current = func_tree;
    gl_value_t *res = gl_value_make_nil();
    while (current->type != GL_AST_NIL) {
        res = gl_eval(current->value.cons.car, env);
        current = current->value.cons.cdr;
    }
    return res;
}

static gl_value_t *__gl_eval_function(gl_function_t *function, gl_function_args_t *args) {
    gl_env_t *local_env = gl_make_env(function->closure);

    if (args) {
        __gl_define_function_args(function, args, local_env);
    }

    gl_value_t *res = __gl_eval_function_tree(function->tree, local_env);
    // Copying value to avoid use after free if desctruction of local_env affected result
    res = gl_value_copy(res);

    gl_env_destroy(local_env);

    return res;
}

static gl_value_t *__gl_eval_list(gl_ast_node_t *node, gl_env_t *env) {
    gl_ast_node_t *func = node->value.cons.car;
    gl_ast_node_t *args = node->value.cons.cdr;

    gl_value_t *func_ptr = NULL;

    // Trying to evaluate expression given instead of function name
    if (func->type != GL_AST_SYMBOL) {
        func_ptr = gl_eval(func, env);
        assert(func_ptr->type == GL_VAL_FUNCTION || func_ptr->type == GL_VAL_BUILTIN ||
               func_ptr->type == GL_VAL_SPFORM);
    } else {
        const char *func_name = func->value.symbol;
        func_ptr = gl_env_get_fun(env, func_name);
        // Evaluating variable value if function is undefined. We trying it because variables can
        // also store functions
        if (func_ptr == NULL) {
            func_ptr = gl_env_get_var(env, func_name);
        }
        // Emmiting error if function still not found
        if (!func_ptr || !func_ptr->val) {
            const char *fmt = "undefined function '%s'";
            char buf[1024];
            snprintf(buf, sizeof(buf), fmt, func_name);
            gl_error_t *err = gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, func->location, buf);
            gl_diagnostic_report_error(err);
            return NULL;
        }
    }

    // Evaluating function
    switch (func_ptr->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_ptr->val;
        gl_function_args_t *argv = __gl_eval_args(args, env);

        if (argv == NULL) {
            return NULL;
        }

        return builtin(node->location, argv);
    }
    case GL_VAL_SPFORM: {
        gl_special_form_t spform = func_ptr->val;

        return spform(env, args);
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_ptr->val;
        gl_function_args_t *argv = __gl_eval_args(args, env);

        if (argv == NULL) {
            return NULL;
        }

        return __gl_eval_function(function, argv);
    }
    default:
        assert(0 && "Function type must be one of these: GL_VAL_FUNCTION, GL_VALUE_BUILTIN, "
                    "GL_VALUE_SPFORM");
    }
    return NULL;
}

gl_value_t *gl_eval(gl_ast_node_t *node, gl_env_t *env) {
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case GL_AST_INT:
        return __gl_eval_int(node);
    case GL_AST_FLOAT:
        return __gl_eval_float(node);
    case GL_AST_SYMBOL:
        return __gl_eval_symbol(node, env);
    case GL_AST_NIL:
        return __gl_eval_nil();
    case GL_AST_CONS:
        return __gl_eval_list(node, env);
    default:
        return NULL;
    }
}

gl_value_t *gl_parse_and_eval(gl_parser_t *restrict parser, gl_env_t *env) {
    gl_ast_node_t *current = gl_parser_parse(parser);

    gl_value_t *res = NULL;

    while (current) {
        res = gl_eval(current, env);
        current = gl_parser_parse(parser);
    }

    return res;
}
