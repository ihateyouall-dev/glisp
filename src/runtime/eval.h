#pragma once

/**
 * @file eval.h
 * @brief Evaluation entry points.
 */

#include "ast.h"
#include "parser.h"
#include "value.h"

/**
 * Evaluates a single AST node.
 *
 * This is the interpreter's safe point: garbage collection may run on entry.
 * Returns NULL on error, after a diagnostic has been reported.
 */
gl_value_t *gl_eval(gl_ast_node_t *node);

/**
 * Parses and evaluates every top-level form produced by @p parser.
 *
 * A failing form is reported and evaluation continues, but the unit as a whole
 * then returns NULL.
 *
 * @return the value of the last form, or NULL if the unit was empty or failed.
 */
gl_value_t *gl_parse_and_eval(gl_parser_t *restrict parser);

/**
 * Evaluates the argument AST list into a newly allocated argument array.
 *
 * Each evaluated argument is pushed as a temporary GC root; the caller must
 * call gl_gc_roots_release with a mark taken before this call.
 */
gl_function_args_t *gl_eval_args(gl_ast_node_t *args);

/** Invokes a builtin or function with an existing argument array. */
gl_value_t *gl_call_function(gl_location_t loc, gl_value_t *func_val, gl_function_args_t *argv);

/**
 * Expands FORM once if its head resolves to a macro.
 *
 * FORM is a runtime value (as returned by quote/read), not an AST. The macro
 * receives the elements of FORM as values. When FORM is not a macro call, it is
 * returned unchanged and *did_expand is set to 0.
 */
gl_value_t *gl_macro_expand_value(gl_value_t *form, gl_location_t loc, int *did_expand);
