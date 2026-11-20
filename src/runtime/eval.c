/**
 * @file eval.c
 * @brief Tree-walking evaluator and function application.
 */

#include "eval.h"

#include "ast.h"
#include "callstack.h"
#include "diagnostics.h"
#include "location.h"
#include "memory.h"
#include "parser.h"
#include "runtime/env.h"
#include "syntax.h"
#include "value.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/resource.h>
#endif

// Deeper nesting than this is reported as an error: the C stack is finite, so
// a runaway Lisp call has to end with a diagnostic instead of a crash.
const size_t GL_MAX_EVAL_DEPTH = 1000;

static size_t gl_eval_depth = 0;
static size_t gl_macro_depth = 0;
static const size_t GL_MAX_MACRO_DEPTH = 1000;
static const size_t GL_MAX_MACRO_EXPANSIONS = 100000;
static uintptr_t gl_stack_base = 0;
static size_t gl_stack_budget = 0;

static size_t __gl_stack_budget(void) {
    if (gl_stack_budget != 0) {
        return gl_stack_budget;
    }

    size_t budget = (size_t)6 * 1024 * 1024;
#ifndef _WIN32
    struct rlimit limit;
    if (getrlimit(RLIMIT_STACK, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY) {
        size_t stack = (size_t)limit.rlim_cur;
        size_t margin = (size_t)1024 * 1024;
        budget = stack > margin ? stack - margin : stack / 2;
    }
#endif

    gl_stack_budget = budget;
    return budget;
}

static int __gl_stack_exhausted(void) {
    volatile unsigned char probe = 0;
    uintptr_t current = (uintptr_t)&probe;

    // Only the numeric position is kept, never a pointer that could be
    // dereferenced, so nothing dangles once this frame returns.
    if (gl_stack_base == 0) {
        gl_stack_base = current; // NOLINT(clang-analyzer-core.StackAddressEscape)
        return 0;                // NOLINT(clang-analyzer-core.StackAddressEscape)
    }

    size_t used = current > gl_stack_base ? (size_t)(current - gl_stack_base)
                                          : (size_t)(gl_stack_base - current);
    return used > __gl_stack_budget();
}

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
        char *suggestion = gl_env_suggest_name((gl_env_t *)env->val, node->value.symbol);
        if (suggestion != NULL) {
            snprintf(buf, sizeof(buf), "undefined variable '%s' (did you mean '%s'?)",
                     node->value.symbol, suggestion);
            free(suggestion);
        } else {
            snprintf(buf, sizeof(buf), "undefined variable '%s'", node->value.symbol);
        }
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
    size_t root_mark = gl_gc_root_mark();
    size_t size = node->value.array.size;
    gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
    gl_value_array_init(arr, size, NULL);
    for (size_t i = 0; i < size; ++i) {
        arr->data[i] = gl_eval(node->value.array.data[i]);
        gl_gc_push_root(arr->data[i]);
    }
    gl_value_t *res = gl_value_make_array_from(arr);
    gl_gc_roots_release(root_mark);
    return res;
}

static gl_value_t *__gl_eval_vector(gl_ast_node_t *node) {
    size_t root_mark = gl_gc_root_mark();
    gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
    gl_value_vector_init(vec, NULL);
    for (size_t i = 0; i < node->value.vector.size; ++i) {
        gl_value_t *elem = gl_eval(*gl_ast_vector_at(&node->value.vector, i));
        gl_gc_push_root(elem);
        gl_value_vector_push_back(vec, elem);
    }
    gl_value_t *res = gl_value_make_vector_from(vec);
    gl_gc_roots_release(root_mark);
    return res;
}

static void __gl_function_arg_destroy(gl_function_arg_t **arg) {
    if (arg && *arg) {
        free(*arg);
        *arg = NULL;
    }
}

// Evaluates arguments left to right and roots each value for the duration of
// the call, because a later argument may trigger a collection.
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
    // Arguments past a failing one are never filled, so the partial array is
    // zeroed to let the cleanup path free it safely.
    memset(res->data, 0, sizeof(gl_function_arg_t *) * argc);

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
        gl_gc_push_root(res->data[i]->val);
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

static gl_value_t *__gl_eval_tail(gl_ast_node_t *node);

// Evaluates the body forms in order; the last one runs in tail position, so a
// recursive call at the end of the body does not grow the C stack.
static gl_value_t *__gl_eval_function_tree(gl_ast_node_t *func_tree, gl_env_t *env) {
    gl_ast_node_t *current = func_tree;
    gl_value_t *res = gl_value_make_nil();

    while (current->type != GL_AST_NIL) {
        if (current->value.cons.cdr->type == GL_AST_NIL) {
            return __gl_eval_tail(current->value.cons.car);
        }
        res = gl_eval(current->value.cons.car);
        if (!res) {
            return NULL;
        }
        current = current->value.cons.cdr;
    }
    return res;
}

// Reports the arity mismatch of a user function call in the same way the
// normal call path does.
static void __gl_report_arity_at(const gl_function_t *function, gl_location_t location,
                                 size_t argc) {
    if (function->has_rest) {
        gl_diagnostic_report_variadic_arity_error(location, function->min_args, argc);
    } else if (function->min_args == function->max_args) {
        gl_diagnostic_report_arity_error(location, function->min_args, argc);
    } else {
        gl_diagnostic_report_range_arity_error(location, function->min_args, function->max_args,
                                               argc);
    }
}

static void __gl_report_arity(const gl_function_t *function, gl_ast_node_t *func_node,
                              const gl_function_args_t *argv) {
    __gl_report_arity_at(function, func_node->location, argv->size);
}

static int __gl_arity_ok(const gl_function_t *function, const gl_function_args_t *argv) {
    return argv->size >= function->min_args &&
           (function->has_rest || argv->size <= function->max_args);
}

static gl_value_t *__gl_eval_function(gl_function_t *function, gl_function_args_t *args);
static gl_ast_node_t *__gl_macro_expand_ast(gl_value_t *macro_val, gl_ast_node_t *args,
                                            gl_location_t location, gl_ast_node_t *call_node);

// Evaluates every form except the last one; the last form is returned so the
// caller can evaluate it in tail position.
static gl_value_t *__gl_eval_leading_forms(gl_ast_node_t *body, gl_ast_node_t **last) {
    gl_value_t *res = gl_value_make_nil();

    while (body != NULL && body->type == GL_AST_CONS) {
        if (body->value.cons.cdr->type == GL_AST_NIL) {
            *last = body->value.cons.car;
            return res;
        }
        res = gl_eval(body->value.cons.car);
        if (!res) {
            return NULL;
        }
        body = body->value.cons.cdr;
    }
    *last = NULL;
    return res;
}

// ASTs produced by macro expansion while evaluating a tail chain. They must
// outlive the loop, so they are collected here and freed when the chain ends.
typedef struct {
    gl_ast_node_t **items;
    size_t size;
    size_t capacity;
} gl_owned_ast_t;

static int __gl_owned_ast_push(gl_owned_ast_t *owned, gl_ast_node_t *ast) {
    if (owned->size == owned->capacity) {
        size_t capacity = owned->capacity == 0 ? 8 : owned->capacity * 2;
        gl_ast_node_t **grown = realloc(owned->items, capacity * sizeof(gl_ast_node_t *));
        if (grown == NULL) {
            return 0;
        }
        owned->items = grown;
        owned->capacity = capacity;
    }
    owned->items[owned->size++] = ast;
    return 1;
}

static void __gl_owned_ast_destroy(gl_owned_ast_t *owned) {
    for (size_t i = 0; i < owned->size; ++i) {
        gl_ast_destroy(owned->items[i]);
    }
    free(owned->items);
}

// Evaluates NODE in tail position. A call to a user function replaces the
// current call frame instead of stacking a new one, and the core forms that
// only select one of their sub-expressions are looked through, so a recursion
// that ends in if/progn runs in constant C stack. Macro expansions continue in
// tail position; their ASTs are kept in OWNED until the chain finishes.
static gl_value_t *__gl_eval_tail_impl(gl_ast_node_t *node, gl_owned_ast_t *owned) {
    while (node != NULL) {
        if (node->type != GL_AST_CONS) {
            return gl_eval(node);
        }

        gl_ast_node_t *head = node->value.cons.car;
        gl_ast_node_t *body = node->value.cons.cdr;

        gl_value_t *env = gl_current_env();
        gl_value_t *callee = NULL;

        if (head->type == GL_AST_SYMBOL) {
            const char *name = head->value.symbol;
            callee = gl_env_get_callable((gl_env_t *)env->val, name);

            if (callee != NULL && callee->type == GL_VAL_SPFORM) {
                if (strcmp(name, "if") == 0) {
                    gl_value_t *test = gl_eval(gl_ast_cons_nth_car(body, 0));
                    if (test == NULL) {
                        return NULL;
                    }

                    gl_ast_node_t *consequent = gl_ast_cons_nth_car(body, 1);
                    gl_ast_node_t *alternates = gl_ast_cons_nth_cdr(body, 1);

                    if (gl_value_get_bool(test)) {
                        node = consequent;
                        continue;
                    }
                    if (alternates == NULL || alternates->type == GL_AST_NIL) {
                        return gl_value_make_nil();
                    }

                    gl_ast_node_t *last = NULL;
                    gl_value_t *res = __gl_eval_leading_forms(alternates, &last);
                    if (res == NULL) {
                        return NULL;
                    }
                    node = last;
                    continue;
                }

                if (strcmp(name, "progn") == 0) {
                    if (body == NULL || body->type == GL_AST_NIL) {
                        return gl_value_make_nil();
                    }

                    gl_ast_node_t *last = NULL;
                    gl_value_t *res = __gl_eval_leading_forms(body, &last);
                    if (res == NULL) {
                        return NULL;
                    }
                    node = last;
                    continue;
                }
            }
        }

        if (callee != NULL && callee->type == GL_VAL_MACRO) {
            if (owned->size >= GL_MAX_MACRO_EXPANSIONS) {
                gl_diagnostic_report_error(gl_make_error(GL_RECURSION_ERROR, GL_ERROR,
                                                         head->location,
                                                         "Maximum macro expansion chain exceeded"));
                return NULL;
            }

            gl_ast_node_t *expansion = __gl_macro_expand_ast(callee, body, head->location, head);
            if (expansion == NULL) {
                return NULL;
            }
            if (!__gl_owned_ast_push(owned, expansion)) {
                gl_ast_destroy(expansion);
                gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, head->location,
                                                         "Out of memory during macro expansion"));
                return NULL;
            }

            node = expansion;
            continue;
        }

        // Everything else is a call or not a call at all. A call to a user
        // function is the only case that can run in constant stack.
        if (head->type != GL_AST_SYMBOL) {
            return gl_eval(node);
        }

        if (callee == NULL || callee->type != GL_VAL_FUNCTION) {
            return gl_eval(node);
        }

        gl_value_t *func_val = callee;
        gl_function_t *function = func_val->val;
        size_t root_mark = gl_gc_root_mark();

        // Arguments are evaluated in the frame that is about to be replaced.
        gl_function_args_t *argv = gl_eval_args(body);
        if (argv == NULL) {
            gl_gc_roots_release(root_mark);
            return NULL;
        }
        if (!__gl_arity_ok(function, argv)) {
            __gl_report_arity(function, head, argv);
            gl_function_args_destroy(argv);
            free(argv);
            gl_gc_roots_release(root_mark);
            return NULL;
        }

        gl_call_stack_pop_call();
        gl_call_stack_push_call(function, head);
        __gl_define_function_args(function, argv, (gl_env_t *)gl_current_env()->val);

        gl_function_args_destroy(argv);
        free(argv);
        gl_gc_roots_release(root_mark);

        gl_ast_node_t *last = NULL;
        gl_value_t *res = __gl_eval_leading_forms(function->tree, &last);
        if (res == NULL) {
            return NULL;
        }
        if (last == NULL) {
            return gl_value_make_nil();
        }
        node = last;
    }

    return gl_value_make_nil();
}

static gl_value_t *__gl_eval_tail(gl_ast_node_t *node) {
    gl_owned_ast_t owned = {0};
    gl_value_t *res = __gl_eval_tail_impl(node, &owned);
    __gl_owned_ast_destroy(&owned);
    return res;
}

// Binds parameters in the call frame environment, then evaluates the body.
static gl_value_t *__gl_eval_function(gl_function_t *function, gl_function_args_t *args) {
    gl_value_t *env = gl_current_env();
    __gl_define_function_args(function, args, (gl_env_t *)env->val);

    gl_value_t *res = __gl_eval_function_tree(function->tree, (gl_env_t *)env->val);
    if (!res) {
        return NULL;
    }

    return res;
}

// --- macro expansion ------------------------------------------------------

// Evaluates the body of a macro with already-built (raw) arguments. The macro
// result is a value representing code, not an AST.
static gl_value_t *__gl_call_macro(gl_value_t *macro_val, gl_function_args_t *argv,
                                   gl_location_t location, gl_ast_node_t *call_node) {
    gl_function_t *macro = gl_value_get_macro(macro_val);
    size_t argc = argv != NULL ? argv->size : 0;

    if (argc < macro->min_args || (!macro->has_rest && argc > macro->max_args)) {
        __gl_report_arity_at(macro, location, argc);
        return NULL;
    }

    if (gl_macro_depth >= GL_MAX_MACRO_DEPTH) {
        gl_diagnostic_report_error(gl_make_error(GL_RECURSION_ERROR, GL_ERROR, location,
                                                 "Maximum macro expansion depth exceeded"));
        return NULL;
    }

    size_t root_mark = gl_gc_root_mark();
    gl_gc_push_root(macro_val);
    if (argv != NULL) {
        for (size_t i = 0; i < argv->size; ++i) {
            gl_gc_push_root(argv->data[i]->val);
        }
    }

    ++gl_macro_depth;
    gl_init_call_stack();
    gl_call_stack_push_call(macro, call_node);

    gl_value_t *res = __gl_eval_function(macro, argv);
    if (!res) {
        gl_call_stack_print_stacktrace();
    } else {
        gl_call_stack_pop_call();
    }

    --gl_macro_depth;
    gl_gc_roots_release(root_mark);
    return res;
}

// Converts unevaluated macro arguments to values. The caller must release its
// root mark after the macro call.
static gl_function_args_t *__gl_macro_args_from_ast(gl_ast_node_t *args) {
    size_t argc = 0;
    for (gl_ast_node_t *cur = args; cur != NULL && cur->type == GL_AST_CONS;
         cur = cur->value.cons.cdr) {
        ++argc;
    }

    gl_function_args_t *res = malloc(sizeof(gl_function_args_t));
    gl_function_args_init(res, argc, gl_function_arg_free);
    if (argc > 0) {
        memset(res->data, 0, sizeof(gl_function_arg_t *) * argc);
    }

    gl_ast_node_t *cur = args;
    for (size_t i = 0; i < argc; ++i, cur = cur->value.cons.cdr) {
        res->data[i] = malloc(sizeof(gl_function_arg_t));
        res->data[i]->location = cur->location;
        res->data[i]->val = gl_ast_to_value(cur->value.cons.car);
        if (res->data[i]->val == NULL) {
            gl_function_args_destroy(res);
            free(res);
            return NULL;
        }
        gl_gc_push_root(res->data[i]->val);
    }

    return res;
}

// Builds an argument array from the elements of a runtime form (macroexpand).
// *proper is cleared for an improper argument list.
static gl_function_args_t *__gl_macro_args_from_value(gl_value_t *form, int *proper) {
    *proper = 1;
    gl_value_t *rest = gl_value_get_cons(form)->cdr;

    size_t argc = 0;
    for (gl_value_t *cur = rest; cur != NULL && cur->type == GL_VAL_CONS;
         cur = gl_value_get_cons(cur)->cdr) {
        ++argc;
    }

    gl_value_t *tail = rest;
    while (tail != NULL && tail->type == GL_VAL_CONS) {
        tail = gl_value_get_cons(tail)->cdr;
    }
    if (tail != NULL && tail->type != GL_VAL_NIL) {
        *proper = 0;
        return NULL;
    }

    gl_function_args_t *res = malloc(sizeof(gl_function_args_t));
    gl_function_args_init(res, argc, gl_function_arg_free);
    if (argc > 0) {
        memset(res->data, 0, sizeof(gl_function_arg_t *) * argc);
    }

    gl_value_t *cur = rest;
    for (size_t i = 0; i < argc; ++i, cur = gl_value_get_cons(cur)->cdr) {
        res->data[i] = malloc(sizeof(gl_function_arg_t));
        res->data[i]->location = (gl_location_t){0};
        res->data[i]->val = gl_value_get_cons(cur)->car;
        gl_gc_push_root(res->data[i]->val);
    }

    return res;
}

// Expands one macro call from unevaluated AST arguments and returns the
// expansion as an AST rooted at the call site.
static gl_ast_node_t *__gl_macro_expand_ast(gl_value_t *macro_val, gl_ast_node_t *args,
                                            gl_location_t location, gl_ast_node_t *call_node) {
    size_t root_mark = gl_gc_root_mark();
    gl_function_args_t *argv = __gl_macro_args_from_ast(args);
    if (argv == NULL) {
        gl_gc_roots_release(root_mark);
        return NULL;
    }

    gl_value_t *expansion = __gl_call_macro(macro_val, argv, location, call_node);
    gl_function_args_destroy(argv);
    free(argv);

    if (expansion == NULL) {
        gl_gc_roots_release(root_mark);
        return NULL;
    }

    gl_gc_push_root(expansion);
    gl_ast_node_t *ast = gl_value_to_ast(expansion, location);
    gl_gc_roots_release(root_mark);
    return ast;
}

gl_value_t *gl_macro_expand_value(gl_value_t *form, gl_location_t location, int *did_expand) {
    if (did_expand != NULL) {
        *did_expand = 0;
    }

    if (form == NULL || form->type != GL_VAL_CONS) {
        return form;
    }

    gl_value_t *head = gl_value_get_cons(form)->car;
    if (head == NULL || head->type != GL_VAL_SYMBOL) {
        return form;
    }

    gl_value_t *env = gl_current_env();
    gl_value_t *callee = gl_env_get_callable((gl_env_t *)env->val, gl_value_get_symbol(head));
    if (callee == NULL || callee->type != GL_VAL_MACRO) {
        return form;
    }

    size_t root_mark = gl_gc_root_mark();
    int proper = 0;
    gl_function_args_t *argv = __gl_macro_args_from_value(form, &proper);
    if (!proper) {
        gl_gc_roots_release(root_mark);
        return form;
    }
    if (argv == NULL) {
        gl_gc_roots_release(root_mark);
        return NULL;
    }

    gl_value_t *res = __gl_call_macro(callee, argv, location, NULL);
    gl_function_args_destroy(argv);
    free(argv);
    gl_gc_roots_release(root_mark);

    if (res != NULL && did_expand != NULL) {
        *did_expand = 1;
    }
    return res;
}

// Resolves the callee, then dispatches on special form, builtin, or function.
static gl_value_t *__gl_eval_list(gl_ast_node_t *node) {
    gl_ast_node_t *func_node = node->value.cons.car;
    gl_ast_node_t *args = node->value.cons.cdr;

    size_t root_mark = gl_gc_root_mark();
    gl_value_t *func_val = NULL;

    gl_value_t *env = gl_current_env();

    // Trying to evaluate expression given instead of function name
    if (func_node->type != GL_AST_SYMBOL) {
        func_val = gl_eval(func_node);
        if (func_val == NULL) {
            return NULL;
        }
        gl_gc_push_root(func_val);
        if (func_val->type != GL_VAL_FUNCTION && func_val->type != GL_VAL_BUILTIN &&
            func_val->type != GL_VAL_SPFORM && func_val->type != GL_VAL_MACRO) {
            gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, func_node->location,
                                                     "Expression result cannot be called"));
            gl_gc_roots_release(root_mark);
            return NULL;
        }
    } else {
        const char *func_name = func_node->value.symbol;
        func_val = gl_env_get_callable((gl_env_t *)env->val, func_name);
        if (!func_val) {
            char buf[1024];
            char *suggestion = gl_env_suggest_name((gl_env_t *)env->val, func_name);
            if (suggestion != NULL) {
                snprintf(buf, sizeof(buf), "undefined function '%s' (did you mean '%s'?)",
                         func_name, suggestion);
                free(suggestion);
            } else {
                snprintf(buf, sizeof(buf), "undefined function '%s'", func_name);
            }
            gl_error_t *err = gl_make_error(GL_SYMBOL_ERROR, GL_ERROR, func_node->location, buf);
            gl_diagnostic_report_error(err);
            return NULL;
        }
        if (func_val->type != GL_VAL_FUNCTION && func_val->type != GL_VAL_BUILTIN &&
            func_val->type != GL_VAL_SPFORM && func_val->type != GL_VAL_MACRO) {
            gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, func_node->location,
                                                     "Expression result cannot be called"));
            return NULL;
        }
        gl_gc_push_root(func_val);
    }

    if (func_val->type == GL_VAL_MACRO) {
        gl_ast_node_t *expansion =
            __gl_macro_expand_ast(func_val, args, func_node->location, func_node);
        if (expansion == NULL) {
            gl_gc_roots_release(root_mark);
            return NULL;
        }

        gl_value_t *res = gl_eval(expansion);
        gl_ast_destroy(expansion);
        gl_gc_roots_release(root_mark);
        return res;
    }

    // Evaluating function
    switch (func_val->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_val->val;
        gl_function_args_t *argv = gl_eval_args(args);

        if (argv == NULL) {
            gl_gc_roots_release(root_mark);
            return NULL;
        }

        gl_value_t *res = builtin(func_node->location, argv);
        gl_function_args_destroy(argv);
        free(argv);
        gl_gc_roots_release(root_mark);
        return res;
    }
    case GL_VAL_SPFORM: {
        gl_special_form_t spform = func_val->val;

        gl_value_t *res = spform((gl_env_t *)env->val, args);
        gl_gc_roots_release(root_mark);
        return res;
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_val->val;
        gl_function_args_t *argv = gl_eval_args(args);

        if (argv == NULL) {
            gl_gc_roots_release(root_mark);
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
            gl_gc_roots_release(root_mark);
            return NULL;
        }

        gl_init_call_stack();
        gl_call_stack_push_call(function, func_node);

        gl_value_t *res = __gl_eval_function(function, argv);

        if (!res) {
            gl_call_stack_print_stacktrace();
            gl_function_args_destroy(argv);
            free(argv);
            gl_gc_roots_release(root_mark);
            return NULL;
        }
        gl_call_stack_pop_call();
        gl_function_args_destroy(argv);
        free(argv);
        gl_gc_roots_release(root_mark);
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

    size_t root_mark = gl_gc_root_mark();
    gl_gc_push_root(func_val);

    if (argv != NULL) {
        for (size_t i = 0; i < argv->size; ++i) {
            gl_gc_push_root(argv->data[i]->val);
        }
    }

    switch (func_val->type) {
    case GL_VAL_BUILTIN: {
        gl_builtin_t builtin = func_val->val;
        gl_value_t *res = builtin(loc, argv);
        gl_gc_roots_release(root_mark);
        return res;
    }
    case GL_VAL_FUNCTION: {
        gl_function_t *function = func_val->val;
        // A caller may pass no argument array at all, which counts as zero
        // arguments instead of dereferencing NULL.
        size_t argc = argv != NULL ? argv->size : 0;
        if (argc < function->min_args || (!function->has_rest && argc > function->max_args)) {
            if (function->has_rest) {
                gl_diagnostic_report_variadic_arity_error(loc, function->min_args, argc);
            } else if (function->min_args == function->max_args) {
                gl_diagnostic_report_arity_error(loc, function->min_args, argc);
            } else {
                gl_diagnostic_report_range_arity_error(loc, function->min_args, function->max_args,
                                                       argc);
            }
            gl_gc_roots_release(root_mark);
            return NULL;
        }
        gl_init_call_stack();
        gl_call_stack_push_call(function, NULL);
        gl_value_t *res = __gl_eval_function(function, argv);
        if (!res) {
            gl_call_stack_print_stacktrace();
            gl_gc_roots_release(root_mark);
            return NULL;
        }
        gl_call_stack_pop_call();
        gl_gc_roots_release(root_mark);
        return res;
    }
    case GL_VAL_MACRO:
        gl_diagnostic_report_error(
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, loc, "Cannot call a macro as a function"));
        gl_gc_roots_release(root_mark);
        return NULL;
    default:
        gl_diagnostic_report_error(
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, loc, "Expression result cannot be called"));
        gl_gc_roots_release(root_mark);
        return NULL;
    }
}

static gl_value_t *__gl_eval_node(gl_ast_node_t *node);

gl_value_t *gl_eval(gl_ast_node_t *node) {
    if (node == NULL) {
        return NULL;
    }

    if (gl_eval_depth == GL_MAX_EVAL_DEPTH) {
        gl_diagnostic_report_error(gl_make_error(GL_RECURSION_ERROR, GL_ERROR, node->location,
                                                 "Maximum recursion depth exceeded"));
        return NULL;
    }

    ++gl_eval_depth;
    gl_value_t *res = __gl_eval_node(node);
    --gl_eval_depth;
    return res;
}

static gl_value_t *__gl_eval_node(gl_ast_node_t *node) {
    if (node == NULL)
        return NULL;

    // Safe point: every value held across evaluation is reachable from an
    // environment, a call frame, or the temporary-root stack.
    gl_gc_maybe_collect();

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

gl_value_t *gl_parse_and_eval(gl_parser_t *restrict parser) {
    gl_value_t *res = NULL;
    int failed = 0;

    while (1) {
        size_t start = parser->pos;
        gl_ast_node_t *current = gl_parser_parse(parser);

        if (!current) {
            // Reaching EOF here is the normal end of the unit; a NULL tree before
            // that means the parser reported a syntax error.
            if (gl_lex_token_vector_at(&parser->tokens, start)->type != GL_LEX_EOF) {
                failed = 1;
            }
            break;
        }

        res = gl_eval(current);
        if (!res) {
            failed = 1;
        }
        gl_ast_destroy(current);
    }

    return failed ? NULL : res;
}
