/**
 * @file value.c
 * @brief Runtime value constructors, accessors, printing, and comparison.
 */

#include "value.h"

#include "ast.h"
#include "env.h"
#include "memory.h"
#include "utils/alloc.h"

#include <inttypes.h>

#ifdef _WIN32
#include "utils/strdup.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gl_value_t *gl_nil = NULL;
gl_value_t *gl_t = NULL;

// Integer and float payloads live in the same allocation as the value header.
// Numeric values are the most common allocation in the interpreter, so one
// block instead of two halves the allocator traffic on arithmetic-heavy code.
static gl_value_t *__gl_value_alloc_inline(gl_value_type_t type, size_t payload_size,
                                           size_t alignment) {
    size_t offset = (sizeof(gl_value_t) + alignment - 1) / alignment * alignment;
    unsigned char *block = gl_xmalloc(offset + payload_size);
    gl_value_t *res = (gl_value_t *)block;

    res->type = type;
    res->val = block + offset;
    return res;
}

gl_value_t *gl_value_make_int(int64_t num) {
    gl_value_t *res = __gl_value_alloc_inline(GL_VAL_INT, sizeof(int64_t), _Alignof(int64_t));
    *(int64_t *)res->val = num;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_bignum(gl_bignum_t *num) {
    int64_t small = 0;
    if (gl_bignum_fits_int64(num, &small)) {
        gl_bignum_free(num);
        return gl_value_make_int(small);
    }

    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_BIGNUM;
    res->val = num;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_float(long double num) {
    gl_value_t *res =
        __gl_value_alloc_inline(GL_VAL_FLOAT, sizeof(long double), _Alignof(long double));
    *(long double *)res->val = num;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_symbol(const char *sym) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_SYMBOL;
    res->val = strdup(sym);
    gl_register_value(res);
    return res;
}

// Converts one AST literal node into a runtime value.
static gl_value_t *__gl_ast_value(gl_ast_node_t *node) {
    if (node == NULL) {
        return gl_value_make_nil();
    }

    switch (node->type) {
    case GL_AST_INT:
        return gl_value_make_int(node->value.integral);
    case GL_AST_BIGNUM:
        return gl_value_make_bignum(gl_bignum_from_string(node->value.bigint));
    case GL_AST_FLOAT:
        return gl_value_make_float(node->value.floating);
    case GL_AST_SYMBOL:
        return gl_value_make_symbol(node->value.symbol);
    case GL_AST_STRING:
        return gl_value_make_string(node->value.string);
    case GL_AST_NIL:
        return gl_value_make_nil();
    case GL_AST_CONS:
        return gl_value_make_cons(node->value.cons);
    case GL_AST_ARRAY: {
        gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
        gl_value_array_init(arr, node->value.array.size, NULL);
        for (size_t i = 0; i < node->value.array.size; ++i) {
            arr->data[i] = __gl_ast_value(node->value.array.data[i]);
        }
        return gl_value_make_array_from(arr);
    }
    case GL_AST_VECTOR: {
        gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
        gl_value_vector_init(vec, NULL);
        for (size_t i = 0; i < node->value.vector.size; ++i) {
            gl_value_vector_push_back(vec,
                                      __gl_ast_value(*gl_ast_vector_at(&node->value.vector, i)));
        }
        return gl_value_make_vector_from(vec);
    }
    }

    return gl_value_make_nil();
}

// Create literal cons of values. The spine is converted iteratively so that
// large quoted lists do not consume the C stack.
gl_value_t *gl_value_make_cons(gl_ast_cons_t cons) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = calloc(1, sizeof(gl_value_cons_t));

    gl_value_cons_t *dst = res->val;
    dst->car = __gl_ast_value(cons.car);

    gl_ast_node_t *cur = cons.cdr;
    while (cur != NULL && cur->type == GL_AST_CONS) {
        gl_value_t *cell = malloc(sizeof(gl_value_t));
        cell->type = GL_VAL_CONS;
        cell->val = calloc(1, sizeof(gl_value_cons_t));
        gl_register_value(cell);

        dst->cdr = cell;
        dst = cell->val;
        dst->car = __gl_ast_value(cur->value.cons.car);
        cur = cur->value.cons.cdr;
    }

    dst->cdr = __gl_ast_value(cur);
    gl_register_value(res);
    return res;
}

// Converts one parsed literal into a value. Quoted arrays and vectors keep
// their element values, which the cons constructor needs because they can
// appear inside a quoted list.
static gl_value_t *__gl_value_from_ast(gl_ast_node_t *node) {
    switch (node->type) {
    case GL_AST_INT:
        return gl_value_make_int(node->value.integral);
    case GL_AST_BIGNUM:
        return gl_value_make_bignum(gl_bignum_from_string(node->value.bigint));
    case GL_AST_FLOAT:
        return gl_value_make_float(node->value.floating);
    case GL_AST_SYMBOL:
        return gl_value_make_symbol(node->value.symbol);
    case GL_AST_STRING:
        return gl_value_make_string(node->value.string);
    case GL_AST_CONS:
        return gl_value_make_cons(node->value.cons);
    case GL_AST_ARRAY: {
        gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
        gl_value_array_init(arr, node->value.array.size, NULL);
        for (size_t i = 0; i < node->value.array.size; ++i) {
            arr->data[i] = __gl_value_from_ast(node->value.array.data[i]);
        }
        return gl_value_make_array_from(arr);
    }
    case GL_AST_VECTOR: {
        gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
        gl_value_vector_init(vec, NULL);
        for (size_t i = 0; i < node->value.vector.size; ++i) {
            gl_value_t *elem = __gl_value_from_ast(*gl_ast_vector_at(&node->value.vector, i));
            gl_value_vector_push_back(vec, elem);
        }
        return gl_value_make_vector_from(vec);
    }
    case GL_AST_NIL:
        break;
    }
    return gl_value_make_nil();
}

gl_value_t *gl_value_make_cons_raw(gl_value_t *car, gl_value_t *cdr) {
    gl_value_cons_t *cons = malloc(sizeof(gl_value_cons_t));
    cons->car = car;
    cons->cdr = cdr;
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = cons;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_string(char *str) { return gl_value_make_string_owned(gl_xstrdup(str)); }

gl_value_t *gl_value_make_string_owned(char *str) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_STRING;
    res->val = str;
    gl_register_value(res);
    return res;
}

// nil and t are process-wide singletons that the collector never frees.
gl_value_t *gl_value_make_nil(void) {
    if (gl_nil == NULL) {
        gl_value_t *nil = calloc(1, sizeof(gl_value_t));
        nil->type = GL_VAL_NIL;
        nil->val = NULL;
        gl_nil = nil;
    }
    return gl_nil;
}

gl_value_t *gl_value_make_bool(int b) {
    if (b) {
        if (gl_t == NULL) {
            gl_value_t *t = calloc(1, sizeof(gl_value_t));
            t->type = GL_VAL_SYMBOL;
            t->val = "t";
            gl_t = t;
        }
        return gl_t;
    } else {
        return gl_value_make_nil();
    }
}

gl_value_t *gl_value_make_typeid(gl_value_type_t type) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_TYPEID;
    res->val = malloc(sizeof(gl_value_type_t));
    if (type == GL_VAL_BUILTIN || type == GL_VAL_SPFORM) {
        type = GL_VAL_FUNCTION;
    }
    *(gl_value_type_t *)res->val = type;
    gl_register_value(res);
    return res;
}

// File payloads are reference-counted because copies share the same handle.
gl_value_t *gl_value_make_file(FILE *handle, char *name, int is_open, gl_file_mode_t mode) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FILE;
    res->val = malloc(sizeof(gl_file_t));
    // The name usually comes from a string value, which the collector may free
    // while the file stays open, so the payload keeps its own copy.
    gl_file_t port =
        (gl_file_t){handle, gl_xstrdup(name != NULL ? name : "<unnamed>"), is_open, mode, 1};
    *(gl_file_t *)res->val = port;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_BUILTIN;
    res->val = builtin;
    gl_register_value(res);
    return res;
}

ARRAY_DEFINE(gl_function_param_t *, gl_function_params);
ARRAY_DEFINE(gl_value_t *, gl_value_array);
VECTOR_DEFINE(gl_value_t *, gl_value_vector);
HASHMAP_DEFINE(gl_value_t *, gl_value_hashmap) // NOLINT(clang-analyzer-core.NullDereference)

gl_function_param_t *gl_function_param_make(const char *name, gl_param_kind_t kind,
                                            gl_ast_node_t *default_val) {
    gl_function_param_t *p = malloc(sizeof(gl_function_param_t));
    p->name = name ? strdup(name) : NULL;
    p->kind = kind;
    p->default_val = default_val;
    return p;
}

gl_function_param_t *gl_function_param_copy(const gl_function_param_t *param) {
    if (!param) {
        return NULL;
    }
    return gl_function_param_make(param->name, param->kind,
                                  param->default_val ? gl_ast_copy(param->default_val) : NULL);
}

void gl_function_param_free(gl_function_param_t **param) {
    if (param && *param) {
        if ((*param)->name) {
            free((*param)->name);
        }
        if ((*param)->default_val) {
            gl_ast_destroy((*param)->default_val);
        }
        free(*param);
        *param = NULL;
    }
}

gl_value_t *gl_value_make_function(const char *name, gl_ast_node_t *tree,
                                   gl_function_params_t *params, gl_value_t *closure) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FUNCTION;
    gl_function_t *func = malloc(sizeof(gl_function_t));
    func->name = name ? strdup(name) : NULL;
    func->tree = tree;
    func->params = params;
    func->closure = closure;
    func->min_args = 0;
    func->max_args = 0;
    func->has_rest = 0;

    if (params) {
        for (size_t i = 0; i < params->size; ++i) {
            gl_function_param_t *p = params->data[i];
            if (p->kind == GL_PARAM_REQUIRED) {
                func->min_args++;
                func->max_args++;
            } else if (p->kind == GL_PARAM_OPTIONAL) {
                func->max_args++;
            } else if (p->kind == GL_PARAM_REST) {
                func->has_rest = 1;
                func->max_args = SIZE_MAX;
            }
        }
    }

    res->val = func;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_macro(const char *name, gl_ast_node_t *tree, gl_function_params_t *params,
                                gl_value_t *closure) {
    gl_value_t *res = gl_value_make_function(name, tree, params, closure);
    res->type = GL_VAL_MACRO;
    return res;
}

void gl_function_arg_free(gl_function_arg_t **arg) {
    if (arg && *arg) {
        free(*arg);
        *arg = NULL;
    }
}

// Deep copy: compound values are duplicated recursively; files share a payload
// and increment its reference count.
gl_value_t *gl_value_copy(gl_value_t *val) {
    if (!val || val == gl_nil || val->type == GL_VAL_NIL) {
        return gl_value_make_nil();
    }
    if (val == gl_t) {
        return gl_value_make_bool(1);
    }

    if (val->type == GL_VAL_INT) {
        gl_value_t *res = __gl_value_alloc_inline(GL_VAL_INT, sizeof(int64_t), _Alignof(int64_t));
        *(int64_t *)res->val = *(int64_t *)val->val;
        gl_register_value(res);
        return res;
    }
    if (val->type == GL_VAL_FLOAT) {
        gl_value_t *res =
            __gl_value_alloc_inline(GL_VAL_FLOAT, sizeof(long double), _Alignof(long double));
        *(long double *)res->val = *(long double *)val->val;
        gl_register_value(res);
        return res;
    }

    gl_value_t *res = malloc(sizeof(gl_value_t));
    assert(val->type == GL_VAL_NIL || val->val);
    res->type = val->type;
    res->gc_marked = val->gc_marked;

    switch (val->type) {
    case GL_VAL_BIGNUM:
        res->val = gl_bignum_copy(val->val);
        break;
    case GL_VAL_TYPEID:
        res->val = malloc(sizeof(gl_value_type_t));
        *(gl_value_type_t *)res->val = *(gl_value_type_t *)val->val;
        break;
    case GL_VAL_CONS: {
        // Copy the spine iteratively: long flat lists are common (argument
        // lists, stdlib HOFs) and must not consume the C stack.
        gl_value_cons_t *src = val->val;
        gl_value_cons_t *dst = malloc(sizeof(gl_value_cons_t));
        res->val = dst;
        dst->car = gl_value_copy(src->car);

        gl_value_t *src_cur = src->cdr;
        while (src_cur != NULL && src_cur->type == GL_VAL_CONS) {
            gl_value_t *cell = malloc(sizeof(gl_value_t));
            cell->type = GL_VAL_CONS;
            cell->val = calloc(1, sizeof(gl_value_cons_t));
            gl_register_value(cell);

            dst->cdr = cell;
            src = src_cur->val;
            dst = cell->val;
            dst->car = gl_value_copy(src->car);
            src_cur = src->cdr;
        }

        dst->cdr = gl_value_copy(src_cur);
        break;
    }
    case GL_VAL_STRING:
    case GL_VAL_SYMBOL:
        res->val = strdup((char *)val->val);
        break;
    case GL_VAL_FUNCTION:
    case GL_VAL_MACRO:
        res->val = malloc(sizeof(gl_function_t));
        gl_function_t *func = res->val;
        gl_function_t *orig = val->val;

        func->name = orig->name ? strdup(orig->name) : NULL;
        func->tree = orig->tree ? gl_ast_copy(orig->tree) : NULL;
        func->closure = orig->closure;
        func->min_args = orig->min_args;
        func->max_args = orig->max_args;
        func->has_rest = orig->has_rest;
        if (orig->params) {
            func->params = malloc(sizeof(gl_function_params_t));
            gl_function_params_init(func->params, orig->params->size, &gl_function_param_free);
            for (size_t i = 0; i < func->params->size; ++i) {
                func->params->data[i] = gl_function_param_copy(orig->params->data[i]);
            }
        } else {
            func->params = NULL;
        }
        break;
    case GL_VAL_BUILTIN:
    case GL_VAL_SPFORM:
    case GL_VAL_ENV:
        res->val = val->val;
        break;
    case GL_VAL_NIL:
        res->val = NULL;
        break;
    case GL_VAL_ARRAY: {
        gl_value_array_t *orig = val->val;
        gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
        gl_value_array_init(arr, orig->size, NULL);
        for (size_t i = 0; i < orig->size; ++i) {
            arr->data[i] = gl_value_copy(orig->data[i]);
        }
        res->val = arr;
        break;
    }
    case GL_VAL_VECTOR: {
        gl_value_vector_t *orig = val->val;
        gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
        gl_value_vector_init(vec, NULL);
        for (size_t i = 0; i < orig->size; ++i) {
            gl_value_vector_push_back(vec, gl_value_copy(*gl_value_vector_at(orig, i)));
        }
        res->val = vec;
        break;
    }
    case GL_VAL_FILE: {
        gl_file_t *file = val->val;
        if (file) {
            file->ref_count++;
        }
        res->val = file;
        break;
    }
    case GL_VAL_HASHMAP: {
        gl_value_hashmap_t *orig = val->val;
        gl_value_hashmap_t *map = malloc(sizeof(gl_value_hashmap_t));
        gl_value_hashmap_init(map, NULL, gl_value_copy);
        for (size_t i = 0; i < orig->buckets_count; ++i) {
            gl_value_hashmapBucket_t *bucket = &orig->buckets[i];
            for (size_t j = 0; j < bucket->size; ++j) {
                gl_value_hashmapEntry_t *entry = gl_value_hashmapBucket_at(bucket, j);
                gl_value_hashmap_insert(map, entry->key, gl_value_copy(entry->value));
            }
        }
        res->val = map;
        break;
    }
    }
    gl_register_value(res);
    return res;
}

// Releases the payload. Files are closed when the last reference is dropped.
void gl_value_destroy(gl_value_t **val) {
    if (val == NULL || *val == NULL)
        return;

    if (*val == gl_nil || *val == gl_t) {
        *val = NULL;
        return;
    }

    switch ((*val)->type) {
    case GL_VAL_BIGNUM:
        gl_bignum_free((*val)->val);
        (*val)->val = NULL;
        break;
    case GL_VAL_INT:
    case GL_VAL_FLOAT:
        // The payload is stored in the value block and is released with it.
        break;
    case GL_VAL_TYPEID:
    case GL_VAL_STRING:
    case GL_VAL_SYMBOL:
        if ((*val)->val == NULL) {
            break;
        }
        free((*val)->val);
        (*val)->val = NULL;
        break;
    case GL_VAL_FUNCTION:
    case GL_VAL_MACRO:
        if ((*val)->val != NULL) {
            gl_function_t *func = (*val)->val;
            if (func->name) {
                free(func->name);
            }
            if (func->tree) {
                gl_ast_destroy(func->tree);
            }
            if (func->params) {
                gl_function_params_destroy(func->params);
                free(func->params);
            }
            free(func);
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_CONS:
        if ((*val)->val != NULL) {
            free((*val)->val);
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_ARRAY:
        if ((*val)->val != NULL) {
            gl_value_array_t *arr = (*val)->val;
            free(arr->data);
            free(arr);
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_VECTOR:
        if ((*val)->val != NULL) {
            gl_value_vector_t *vec = (*val)->val;
            free(vec->data);
            free(vec);
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_HASHMAP:
        if ((*val)->val != NULL) {
            gl_value_hashmap_t *map = (*val)->val;
            gl_value_hashmap_destroy(map);
            free(map);
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_FILE:
        if ((*val)->val != NULL) {
            gl_file_t *file = gl_value_get_file(*val);
            if (--file->ref_count <= 0) {
                if (file->is_open && file->handle && file->handle != stdin &&
                    file->handle != stdout && file->handle != stderr) {
                    fclose(file->handle);
                    file->is_open = 0;
                }
                free(file->name);
                file->name = NULL;
                free(file);
            }
            (*val)->val = NULL;
        }
        break;
    case GL_VAL_ENV:
        if ((*val)->val == NULL) {
            break;
        }
        gl_env_destroy((*val)->val);
        (*val)->val = NULL;
        break;
    case GL_VAL_BUILTIN:
    case GL_VAL_SPFORM:
    case GL_VAL_NIL:
        break;
    }
    free(*val);
    *val = NULL;
}

gl_value_t *gl_value_make_specform(gl_special_form_t spform) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_SPFORM;
    res->val = spform;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_env(gl_env_t *env) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_ENV;
    res->val = env;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_array(size_t size) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_ARRAY;
    gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
    gl_value_array_init(arr, size, NULL);
    for (size_t i = 0; i < size; ++i) {
        arr->data[i] = gl_value_make_nil();
    }
    res->val = arr;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_array_from(gl_value_array_t *arr) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_ARRAY;
    res->val = arr;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_vector(void) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_VECTOR;
    gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
    gl_value_vector_init(vec, NULL);
    res->val = vec;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_vector_from(gl_value_vector_t *vec) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_VECTOR;
    res->val = vec;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_hashmap(void) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_HASHMAP;
    gl_value_hashmap_t *map = malloc(sizeof(gl_value_hashmap_t));
    gl_value_hashmap_init(map, NULL, gl_value_copy);
    res->val = map;
    gl_register_value(res);
    return res;
}

static const char *__gl_typeid_str(gl_value_type_t type) {
    switch (type) {
    case GL_VAL_INT:
        return "int";
    case GL_VAL_BIGNUM:
        return "bignum";
    case GL_VAL_FLOAT:
        return "float";
    case GL_VAL_TYPEID:
        return "typeid";
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
    case GL_VAL_MACRO:
        return "macro";
    case GL_VAL_NIL:
        return "nil";
    case GL_VAL_ARRAY:
        return "array";
    case GL_VAL_VECTOR:
        return "vector";
    case GL_VAL_FILE:
        return "file";
    case GL_VAL_HASHMAP:
        return "hashmap";
    case GL_VAL_ENV:
        assert(0 && "UNREACHABLE");
        return "environment";
    }
    return "unknown";
}

// Textual representation used by print/write. Strings and symbols are emitted
// verbatim; improper lists use dotted-pair notation.
// Floats print with the shortest form that keeps 15 significant digits, so
// 1.5 stays 1.5 instead of 1.500000, and an integral value keeps its type
// visible as 1.0. Very large or small magnitudes still use exponent notation.
static void __gl_fprint_float(FILE *file, long double num) {
    char buf[64];

    snprintf(buf, sizeof(buf), "%.15Lg", num);
    if (strpbrk(buf, ".eE") == NULL) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof(buf) - len, ".0");
    }
    fputs(buf, file);
}

void gl_value_fprint(FILE *file, gl_value_t *val) {
    if (!val) {
        return;
    }
    switch (val->type) {
    case GL_VAL_INT:
        fprintf(file, "%" PRId64, (*(int64_t *)val->val));
        break;
    case GL_VAL_BIGNUM: {
        char *text = gl_bignum_to_string(val->val);
        fputs(text, file);
        free(text);
        break;
    }
    case GL_VAL_FLOAT:
        __gl_fprint_float(file, *(long double *)val->val);
        break;
    case GL_VAL_TYPEID:
        fprintf(file, "%s", __gl_typeid_str(*(gl_value_type_t *)val->val));
        break;
    case GL_VAL_STRING:
    case GL_VAL_SYMBOL:
        fprintf(file, "%s", (char *)val->val);
        break;
    case GL_VAL_NIL:
        fprintf(file, "nil");
        break;
    case GL_VAL_BUILTIN:
    case GL_VAL_FUNCTION:
    case GL_VAL_SPFORM:
    case GL_VAL_ENV:
        // Nothing to print now
        break;
    case GL_VAL_MACRO: {
        gl_function_t *macro = val->val;
        fprintf(file, "#<macro %s>", macro && macro->name ? macro->name : "anonymous");
        break;
    }
    case GL_VAL_CONS: {
        fprintf(file, "(");
        gl_value_t *cur = val;
        int first = 1;
        while (cur && cur->type == GL_VAL_CONS) {
            if (!first) {
                fprintf(file, " ");
            }
            first = 0;
            gl_value_cons_t *cons = cur->val;
            gl_value_fprint(file, cons->car);
            cur = cons->cdr;
        }
        if (cur && cur->type != GL_VAL_NIL) {
            fprintf(file, " . ");
            gl_value_fprint(file, cur);
        }
        fprintf(file, ")");
        break;
    }
    case GL_VAL_ARRAY: {
        fprintf(file, "[");
        gl_value_array_t *arr = val->val;
        if (arr && arr->size > 0) {
            for (size_t i = 0; i < arr->size; ++i) {
                gl_value_fprint(file, arr->data[i]);
                if (i + 1 < arr->size) {
                    fprintf(file, " ");
                }
            }
        }
        fprintf(file, "]");
        break;
    }
    case GL_VAL_VECTOR: {
        fprintf(file, "{");
        gl_value_vector_t *vec = val->val;
        if (vec && vec->size > 0) {
            for (size_t i = 0; i < vec->size; ++i) {
                gl_value_fprint(file, *gl_value_vector_at(vec, i));
                if (i + 1 < vec->size) {
                    fprintf(file, " ");
                }
            }
        }
        fprintf(file, "}");
        break;
    }
    case GL_VAL_HASHMAP: {
        gl_value_hashmap_t *map = val->val;
        fprintf(file, "#hashmap(%zu)", map ? map->size : 0);
        break;
    }
    case GL_VAL_FILE: {
        gl_file_t *f = gl_value_get_file(val);
        fprintf(file, "#<file %s %s>", f->name ? f->name : "unnamed",
                f->is_open ? "open" : "closed");
        break;
    }
    }
}
void gl_value_print(gl_value_t *val) { gl_value_fprint(stdout, val); }
char *gl_value_to_string(gl_value_t *val) {
#ifdef _WIN32
    // open_memstream is not available on Windows; render through a temporary
    // file and copy the result back into memory.
    FILE *mem = tmpfile();
    if (mem == NULL) {
        return strdup("");
    }

    gl_value_fprint(mem, val);
    long size = ftell(mem);
    if (size < 0) {
        fclose(mem);
        return strdup("");
    }

    rewind(mem);
    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(mem);
        return strdup("");
    }

    size_t read = fread(buf, 1, (size_t)size, mem);
    buf[read] = '\0';
    fclose(mem);
    return buf;
#else
    char *buf = NULL;
    size_t size = 0;

    FILE *mem = open_memstream(&buf, &size);
    gl_value_fprint(mem, val);

    fclose(mem);
    return buf;
#endif
}

int64_t gl_value_get_int(gl_value_t *val) {
    if (val->type == GL_VAL_FLOAT) {
        return (int64_t)gl_value_get_float(val);
    }
    assert(val->type == GL_VAL_INT);
    return *(int64_t *)val->val;
}

gl_bignum_t *gl_value_get_bignum(gl_value_t *val) {
    assert(val->type == GL_VAL_BIGNUM);
    return val->val;
}

long double gl_value_get_float(gl_value_t *val) {
    if (val->type == GL_VAL_INT) {
        return (long double)gl_value_get_int(val);
    }
    if (val->type == GL_VAL_BIGNUM) {
        return gl_bignum_to_float(val->val);
    }
    assert(val->type == GL_VAL_FLOAT);
    return *(long double *)val->val;
}
char *gl_value_get_symbol(gl_value_t *val) {
    assert(val->type == GL_VAL_SYMBOL);
    return val->val;
}
char *gl_value_get_string(gl_value_t *val) {
    assert(val->type == GL_VAL_STRING);
    return val->val;
}
gl_value_cons_t *gl_value_get_cons(gl_value_t *val) {
    assert(val->type == GL_VAL_CONS);
    return val->val;
}
gl_value_array_t *gl_value_get_array(gl_value_t *val) {
    assert(val->type == GL_VAL_ARRAY);
    return val->val;
}
gl_value_vector_t *gl_value_get_vector(gl_value_t *val) {
    assert(val->type == GL_VAL_VECTOR);
    return val->val;
}
gl_value_hashmap_t *gl_value_get_hashmap(gl_value_t *val) {
    assert(val->type == GL_VAL_HASHMAP);
    return val->val;
}
gl_value_type_t gl_value_get_typeid(gl_value_t *val) {
    assert(val->type == GL_VAL_TYPEID);
    return *(gl_value_type_t *)val->val;
}
gl_file_t *gl_value_get_file(gl_value_t *val) {
    assert(val->type == GL_VAL_FILE);
    return val->val;
}

gl_builtin_t gl_value_get_builtin(gl_value_t *val) {
    assert(val->type == GL_VAL_BUILTIN);
    return val->val;
}
gl_function_t *gl_value_get_function(gl_value_t *val) {
    assert(val->type == GL_VAL_FUNCTION);
    return val->val;
}

gl_function_t *gl_value_get_macro(gl_value_t *val) {
    assert(val->type == GL_VAL_MACRO);
    return val->val;
}
gl_special_form_t gl_value_get_specform(gl_value_t *val) {
    assert(val->type == GL_VAL_SPFORM);
    return val->val;
}
int gl_value_get_bool(gl_value_t *val) {
    // Everything except NIL is interpreted as true
    if (val->type != GL_VAL_NIL) {
        return 1;
    }
    return 0;
}
void gl_value_set_int(gl_value_t *val, int64_t num) {
    if (val->type != GL_VAL_INT) {
        val->type = GL_VAL_INT;
    }
    *(int64_t *)val->val = num;
}
void gl_value_set_float(gl_value_t *val, long double num) {
    if (val->type != GL_VAL_FLOAT) {
        val->type = GL_VAL_FLOAT;
    }
    *(long double *)val->val = num;
}
void gl_value_set_symbol(gl_value_t *val, char *sym) {
    if (val->type != GL_VAL_SYMBOL) {
        val->type = GL_VAL_SYMBOL;
    }
    val->val = strdup(sym);
}
void gl_value_set_builtin(gl_value_t *val, gl_builtin_t builtin) {
    if (val->type != GL_VAL_BUILTIN) {
        val->type = GL_VAL_BUILTIN;
    }
    val->val = builtin;
}
void gl_value_set_function(gl_value_t *val, gl_function_t *func) {
    if (val->type != GL_VAL_FUNCTION) {
        val->type = GL_VAL_FUNCTION;
    }
    val->val = func;
}
void gl_value_set_specform(gl_value_t *val, gl_special_form_t specform) {
    if (val->type != GL_VAL_SPFORM) {
        val->type = GL_VAL_SPFORM;
    }
    val->val = specform;
}

static int __gl_value_is_number(gl_value_t *val) {
    return val->type == GL_VAL_INT || val->type == GL_VAL_BIGNUM || val->type == GL_VAL_FLOAT;
}

static int __gl_compare_ld(long double lhs, long double rhs) {
    if (lhs == rhs) {
        return 0;
    }
    return lhs > rhs ? 1 : -1;
}

static int __gl_value_compare_numbers(gl_value_t *lhs, gl_value_t *rhs) {
    if (lhs->type == GL_VAL_BIGNUM) {
        if (rhs->type == GL_VAL_BIGNUM) {
            return gl_bignum_compare(lhs->val, rhs->val);
        }
        if (rhs->type == GL_VAL_INT) {
            return gl_bignum_compare_int(lhs->val, gl_value_get_int(rhs));
        }
        return __gl_compare_ld(gl_bignum_to_float(lhs->val), gl_value_get_float(rhs));
    }
    if (rhs->type == GL_VAL_BIGNUM) {
        if (lhs->type == GL_VAL_INT) {
            return -gl_bignum_compare_int(rhs->val, gl_value_get_int(lhs));
        }
        return __gl_compare_ld(gl_value_get_float(lhs), gl_bignum_to_float(rhs->val));
    }
    if (lhs->type == GL_VAL_INT && rhs->type == GL_VAL_INT) {
        int64_t lhs_val = gl_value_get_int(lhs);
        int64_t rhs_val = gl_value_get_int(rhs);
        return lhs_val == rhs_val ? 0 : (lhs_val > rhs_val ? 1 : -1);
    }
    return __gl_compare_ld(gl_value_get_float(lhs), gl_value_get_float(rhs));
}

// Structural equality: numbers by value (mixed int/float allowed), symbols and
// strings by content, collections element-wise; hash maps only by identity.
int gl_value_compare(gl_value_t *lhs, gl_value_t *rhs) {
    if (__gl_value_is_number(lhs)) {
        if (__gl_value_is_number(rhs)) {
            return __gl_value_compare_numbers(lhs, rhs);
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_TYPEID) {
        if (rhs->type == GL_VAL_TYPEID) {
            if (*(gl_value_type_t *)lhs->val == *(gl_value_type_t *)rhs->val) {
                return 0;
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_SYMBOL) {
        if (rhs->type == GL_VAL_SYMBOL) {
            return strcmp(gl_value_get_symbol(lhs), gl_value_get_symbol(rhs));
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_NIL || rhs->type == GL_VAL_NIL) {
        if (lhs->type == rhs->type) {
            return 0;
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_STRING) {
        if (rhs->type == GL_VAL_STRING) {
            return strcmp((char *)lhs->val, (char *)rhs->val);
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_ARRAY) {
        if (rhs->type == GL_VAL_ARRAY) {
            gl_value_array_t *a = lhs->val;
            gl_value_array_t *b = rhs->val;
            if (a->size != b->size) {
                return (a->size > b->size) ? 1 : -1;
            }
            for (size_t i = 0; i < a->size; ++i) {
                int cmp = gl_value_compare(a->data[i], b->data[i]);
                if (cmp != 0)
                    return cmp;
            }
            return 0;
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_VECTOR) {
        if (rhs->type == GL_VAL_VECTOR) {
            gl_value_vector_t *a = lhs->val;
            gl_value_vector_t *b = rhs->val;
            if (a->size != b->size) {
                return (a->size > b->size) ? 1 : -1;
            }
            for (size_t i = 0; i < a->size; ++i) {
                int cmp = gl_value_compare(*gl_value_vector_at(a, i), *gl_value_vector_at(b, i));
                if (cmp != 0)
                    return cmp;
            }
            return 0;
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_HASHMAP) {
        return lhs == rhs ? 0 : -1;
    }

    if (lhs->type == GL_VAL_CONS) {
        if (rhs->type == GL_VAL_CONS) {
            gl_value_t *cur_l = lhs;
            gl_value_t *cur_r = rhs;
            while (cur_l && cur_l->type == GL_VAL_CONS && cur_r && cur_r->type == GL_VAL_CONS) {
                gl_value_cons_t *cons_l = cur_l->val;
                gl_value_cons_t *cons_r = cur_r->val;
                int cmp = gl_value_compare(cons_l->car, cons_r->car);
                if (cmp != 0) {
                    return cmp;
                }
                cur_l = cons_l->cdr;
                cur_r = cons_r->cdr;
            }
            return gl_value_compare(cur_l, cur_r);
        } else {
            return -1;
        }
    }

    if (lhs->type == GL_VAL_BUILTIN || lhs->type == GL_VAL_SPFORM || lhs->type == GL_VAL_FUNCTION ||
        lhs->type == GL_VAL_MACRO) {
        if (lhs->type == rhs->type) {
            if (lhs->type == GL_VAL_FUNCTION || lhs->type == GL_VAL_MACRO) {
                gl_function_t *f1 = lhs->type == GL_VAL_MACRO ? gl_value_get_macro(lhs)
                                                              : gl_value_get_function(lhs);
                gl_function_t *f2 = rhs->type == GL_VAL_MACRO ? gl_value_get_macro(rhs)
                                                              : gl_value_get_function(rhs);
                if (f1 == f2) {
                    return 0;
                }
                if (f1->name && f2->name) {
                    return strcmp(f1->name, f2->name);
                }
                return -1;
            }
            if (lhs->val == rhs->val) {
                return 0;
            }
        }
        return -1;
    }
    return -1;
}
