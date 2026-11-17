/**
 * @file syntax.c
 * @brief AST <-> runtime value conversions used by read, eval, and macros.
 */

#include "syntax.h"

#include "diagnostics.h"
#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Converts parsed syntax into runtime data. */
gl_value_t *gl_ast_to_value(gl_ast_node_t *node) {
    if (node == NULL) {
        return NULL;
    }

    switch (node->type) {
    case GL_AST_INT:
        return gl_value_make_int(node->value.integral);
    case GL_AST_FLOAT:
        return gl_value_make_float(node->value.floating);
    case GL_AST_SYMBOL:
        return gl_value_make_symbol(node->value.symbol);
    case GL_AST_STRING:
        return gl_value_make_string(node->value.string);
    case GL_AST_NIL:
        return gl_value_make_nil();
    case GL_AST_CONS:
        return gl_value_make_cons_raw(gl_ast_to_value(node->value.cons.car),
                                      gl_ast_to_value(node->value.cons.cdr));
    case GL_AST_ARRAY: {
        gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
        gl_value_array_init(arr, node->value.array.size, NULL);

        for (size_t i = 0; i < node->value.array.size; ++i) {
            arr->data[i] = gl_ast_to_value(node->value.array.data[i]);
        }
        return gl_value_make_array_from(arr);
    }
    case GL_AST_VECTOR: {
        gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
        gl_value_vector_init(vec, NULL);

        for (size_t i = 0; i < node->value.vector.size; ++i) {
            gl_value_t *elem = gl_ast_to_value(*gl_ast_vector_at(&node->value.vector, i));
            gl_value_vector_push_back(vec, elem);
        }
        return gl_value_make_vector_from(vec);
    }
    }

    return NULL;
}

static void __gl_emit_syntax_value_error(gl_location_t location, const char *type) {
    char buf[256] = "";
    snprintf(buf, sizeof(buf), "Cannot evaluate %s", type);
    gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, location, buf));
}

// gl_ast_make_string decodes escapes, so backslashes in a value are doubled first.
static char *__gl_escape_backslashes(const char *str) {
    size_t len = strlen(str);
    char *res = malloc(len * 2 + 1);
    char *dst = res;

    for (const char *src = str; *src != '\0'; ++src) {
        if (*src == '\\') {
            *dst++ = '\\';
        }
        *dst++ = *src;
    }
    *dst = '\0';
    return res;
}

static const char *__gl_value_type_name(gl_value_t *val) {
    switch (val->type) {
    case GL_VAL_NIL:
        return "nil";
    case GL_VAL_INT:
        return "int";
    case GL_VAL_FLOAT:
        return "float";
    case GL_VAL_SYMBOL:
        return "symbol";
    case GL_VAL_STRING:
        return "string";
    case GL_VAL_CONS:
        return "list";
    case GL_VAL_FUNCTION:
    case GL_VAL_BUILTIN:
    case GL_VAL_SPFORM:
        return "function";
    case GL_VAL_ENV:
        return "environment";
    case GL_VAL_ARRAY:
        return "array";
    case GL_VAL_VECTOR:
        return "vector";
    case GL_VAL_TYPEID:
        return "typeid";
    case GL_VAL_FILE:
        return "file";
    case GL_VAL_HASHMAP:
        return "hashmap";
    case GL_VAL_MACRO:
        return "macro";
    }

    return "value";
}

// A dotted tail cannot be code: only proper lists convert to an AST list.
static gl_ast_node_t *__gl_value_to_ast_list(gl_value_t *val, gl_location_t location) {
    if (val->type == GL_VAL_NIL) {
        return gl_ast_make_nil(location);
    }

    if (val->type != GL_VAL_CONS) {
        __gl_emit_syntax_value_error(location, "improper list");
        return NULL;
    }

    gl_value_cons_t *cons = gl_value_get_cons(val);
    gl_ast_node_t *car = gl_value_to_ast(cons->car, location);
    if (!car) {
        return NULL;
    }

    gl_ast_node_t *cdr = __gl_value_to_ast_list(cons->cdr, location);
    if (!cdr) {
        gl_ast_destroy(car);
        return NULL;
    }

    return gl_ast_make_cons(car, cdr, location);
}

/** Converts runtime data into syntax for evaluation. */
gl_ast_node_t *gl_value_to_ast(gl_value_t *val, gl_location_t location) {
    if (val == NULL) {
        return NULL;
    }

    switch (val->type) {
    case GL_VAL_NIL:
        return gl_ast_make_nil(location);
    case GL_VAL_INT:
        return gl_ast_make_int(gl_value_get_int(val), location);
    case GL_VAL_FLOAT:
        return gl_ast_make_float(gl_value_get_float(val), location);
    case GL_VAL_SYMBOL:
        return gl_ast_make_symbol(gl_value_get_symbol(val), location);
    case GL_VAL_STRING: {
        char *str = __gl_escape_backslashes(gl_value_get_string(val));
        gl_ast_node_t *res = gl_ast_make_string(str, location);
        free(str);
        return res;
    }
    case GL_VAL_CONS:
        return __gl_value_to_ast_list(val, location);
    case GL_VAL_ARRAY: {
        gl_value_array_t *arr = gl_value_get_array(val);
        gl_ast_vector_t temp;
        gl_ast_vector_init(&temp, gl_ast_node_destroy_helper);

        for (size_t i = 0; i < arr->size; ++i) {
            gl_ast_node_t *elem = gl_value_to_ast(arr->data[i], location);
            if (!elem) {
                gl_ast_vector_destroy(&temp);
                return NULL;
            }
            gl_ast_vector_push_back(&temp, elem);
        }
        gl_ast_array_t res;
        gl_ast_array_init(&res, temp.size, gl_ast_node_destroy_helper);
        for (size_t i = 0; i < temp.size; ++i) {
            res.data[i] = temp.data[i];
        }
        free(temp.data);
        return gl_ast_make_array(res, location);
    }
    case GL_VAL_VECTOR: {
        gl_value_vector_t *vec = gl_value_get_vector(val);
        gl_ast_vector_t res;
        gl_ast_vector_init(&res, gl_ast_node_destroy_helper);

        for (size_t i = 0; i < vec->size; ++i) {
            gl_ast_node_t *elem = gl_value_to_ast(*gl_value_vector_at(vec, i), location);
            if (!elem) {
                gl_ast_vector_destroy(&res);
                return NULL;
            }
            gl_ast_vector_push_back(&res, elem);
        }
        return gl_ast_make_vector(res, location);
    }
    default:
        __gl_emit_syntax_value_error(location, __gl_value_type_name(val));
        return NULL;
    }
}
