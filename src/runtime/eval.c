#include "eval.h"
#include "ast.h"
#include "parser.h"
#include "runtime/env.h"
#include "value.h"
#include <stdlib.h>

static gl_value_t *__gl_eval_int(gl_ast_node_t *node) {
    return gl_value_make_int(node->value.integral);
}

static gl_value_t *__gl_eval_float(gl_ast_node_t *node) {
    return gl_value_make_float(node->value.floating);
}

static gl_value_t *__gl_eval_symbol(gl_ast_node_t *node, gl_env_t *env) {
    if (node->quoted) {
        return gl_value_make_symbol(node->value.symbol);
    }
    return gl_env_get_var(env, node->value.symbol);
}

static gl_value_t *__gl_eval_nil(void) { return gl_value_make_nil(); }

static gl_value_array_t *__gl_eval_args(gl_ast_node_t *args, gl_env_t *env) {
    // If have no args, return nothing
    if (args->type == GL_AST_NIL) {
        return NULL;
    }

    size_t argc = 0;
    gl_ast_node_t *current = args;

    while (current->value.cons.cdr->type != GL_AST_NIL) {
        current = current->value.cons.cdr;
        ++argc;
    }

    gl_value_array_t *res = malloc(sizeof(gl_value_array_t));

    gl_value_array_init(res, argc, &gl_value_destroy);

    current = args;
    for (size_t i = 0; i < argc; ++i, current = current->value.cons.cdr) {
        res->data[i] = gl_eval(current->value.cons.car, env);
    }
    return res;
}

static void __gl_define_function_args(const gl_function_t *function, gl_value_array_t *args,
                                      gl_env_t *env) {
    for (size_t i = 0; i < function->params->size; ++i) {
        gl_env_set_var(env, function->params->data[i], args->data[i]);
    }
}

static gl_value_t *__gl_eval_function_tree(gl_ast_node_t *func_tree, gl_env_t *env) {
    gl_ast_node_t *current = func_tree;
    gl_value_t *res = NULL;
    while (current->value.cons.cdr->type != GL_AST_NIL) {
        res = gl_eval(current->value.cons.car, env);
        current = current->value.cons.cdr;
    }
    return res;
}

static gl_value_t *__gl_eval_function(gl_function_t *function, gl_value_array_t *args) {
    assert(function->tree->type == GL_AST_CONS);

    gl_env_t *local_env = malloc(sizeof(gl_env_t));

    local_env->parent = function->closure;

    if (args) {
        __gl_define_function_args(function, args, local_env);
    }

    gl_value_t *res = __gl_eval_function_tree(function->tree, local_env);
    // Copying value to avoid use after free if desctruction of local_env affected result
    res = gl_value_copy(res);

    gl_env_destroy(local_env);
    free(local_env);

    return res;
}

static gl_value_t *__gl_eval_list(gl_ast_node_t *node, gl_env_t *env) {
    if (node->quoted) {
        return gl_value_make_cons(node->value.cons);
    }
    // Otherwise list is evaluated as function
    gl_ast_node_t *func = node->value.cons.car;
    gl_ast_node_t *args = node->value.cons.cdr;

    assert(func->type == GL_AST_SYMBOL);

    const char *func_name = func->value.symbol;

    gl_value_t *func_ptr = gl_env_get_fun(env, func_name);

    // Evaluating function
    switch (func_ptr->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_ptr->val;
        gl_value_array_t *argv = __gl_eval_args(args, env);

        return builtin(argv);
    }
    case GL_VAL_SPFORM: {
        gl_special_form_t spform = func_ptr->val;

        return spform(env, args);
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_ptr->val;
        gl_value_array_t *argv = __gl_eval_args(args, env);

        return __gl_eval_function(function, argv);
    }
    default:
        assert(0 && "UNREACHABLE");
    }
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

gl_value_t *gl_parse_and_eval(const char *restrict src, gl_env_t *env) {
    gl_parser_t parser = gl_make_parser(src);

    gl_ast_node_t *current = gl_parser_parse(&parser);

    gl_value_t *res = NULL;

    while (current) {
        res = gl_eval(current, env);
    }

    return res;
}
