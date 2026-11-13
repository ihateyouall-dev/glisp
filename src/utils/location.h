#pragma once

/**
 * @file location.h
 * @brief Source locations attached to tokens, AST nodes, and values.
 */

#include <stddef.h>

/**
 * A position in a source unit.
 *
 * Tokens, AST nodes, and runtime arguments carry a location so that
 * diagnostics can render the offending source line with a caret.
 */
typedef struct gl_location_t {
    size_t pos;      /**< Byte offset in @ref src. */
    size_t line;     /**< 1-based line number. */
    size_t column;   /**< 1-based column number. */
    char *unit_name; /**< Display name of the source unit (file or "<repl>"). */
    char *src;       /**< Source buffer; borrowed, owned by the source registry. */
} gl_location_t;
