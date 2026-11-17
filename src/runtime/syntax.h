#pragma once

/**
 * @file syntax.h
 * @brief Conversions between parsed AST nodes and runtime data.
 *
 * These helpers implement the "code as data" boundary used by read, eval, and
 * macro expansion. Values returned by gl_ast_to_value are GC-managed; AST trees
 * returned by gl_value_to_ast are owned by the caller and must be destroyed
 * with gl_ast_destroy.
 */

#include "ast.h"
#include "location.h"
#include "value.h"

/**
 * Converts parsed syntax into runtime data.
 *
 * Symbols become symbol values, lists become cons cells, and arrays/vectors
 * become their runtime equivalents.
 *
 * @return a new GC-managed value, or NULL when @p node is NULL.
 */
gl_value_t *gl_ast_to_value(gl_ast_node_t *node);

/**
 * Converts runtime data back into syntax for evaluation.
 *
 * Only proper lists are accepted as list syntax. Functions, builtins, special
 * forms, environments, and files cannot be represented as syntax.
 *
 * @param location source location assigned to every produced AST node; macro
 *                 expansion typically passes the macro call site.
 * @return a newly allocated AST tree, or NULL after reporting an error.
 *         The caller owns the tree.
 */
gl_ast_node_t *gl_value_to_ast(gl_value_t *val, gl_location_t location);
