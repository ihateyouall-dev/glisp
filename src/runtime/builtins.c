/**
 * @file builtins.c
 * @brief Native (C) functions callable from GLisp.
 */

#include "builtins.h"

#include "callstack.h"
#include "diagnostics.h"
#include "env.h"
#include "eval.h"
#include "location.h"
#include "memory.h"
#include "parser.h"
#include "runtime/value.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _MSC_VER
#include "utils/strdup.h"
#endif

ARRAY_DEFINE(gl_function_arg_t *, gl_function_args)

#define EXPECT_ARGS(n)                                                                             \
    do {                                                                                           \
        if (args->size != n) {                                                                     \
            gl_diagnostic_report_arity_error(called_at, n, args->size);                            \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_VARGS(n)                                                                            \
    do {                                                                                           \
        if (args->size < n) {                                                                      \
            gl_diagnostic_report_variadic_arity_error(called_at, n, args->size);                   \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_RVARGS(min, max)                                                                    \
    do {                                                                                           \
        if (args->size < min || args->size > max) {                                                \
            gl_diagnostic_report_range_arity_error(called_at, min, max, args->size);               \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_INT(val)                                                                            \
    do {                                                                                           \
        if (val->type != GL_VAL_INT) {                                                             \
            __emit_type_error(called_at, "int", __gl_value_get_type_name(val));                    \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT(val)                                                                          \
    do {                                                                                           \
        if (val->type != GL_VAL_FLOAT) {                                                           \
            __emit_type_error(called_at, "float", __gl_value_get_type_name(val));                  \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_NUMBER(val)                                                                         \
    do {                                                                                           \
        if (val->type != GL_VAL_INT && val->type != GL_VAL_FLOAT) {                                \
            __emit_type_error(called_at, "number", __gl_value_get_type_name(val));                 \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_SYMBOL(val)                                                                         \
    do {                                                                                           \
        if (val->type != GL_VAL_SYMBOL) {                                                          \
            __emit_type_error(called_at, "symbol", __gl_value_get_type_name(val));                 \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_STRING(val)                                                                         \
    do {                                                                                           \
        if (val->type != GL_VAL_STRING) {                                                          \
            __emit_type_error(called_at, "string", __gl_value_get_type_name(val));                 \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_SYMBOL_OR_STRING(val)                                                               \
    do {                                                                                           \
        if (val->type != GL_VAL_SYMBOL && val->type != GL_VAL_STRING) {                            \
            __emit_type_error(called_at, "string or symbol", __gl_value_get_type_name(val));       \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_LIST(val)                                                                           \
    do {                                                                                           \
        if (val->type != GL_VAL_CONS) {                                                            \
            __emit_type_error(called_at, "list", __gl_value_get_type_name(val));                   \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_ARRAY(val)                                                                          \
    do {                                                                                           \
        if (val->type != GL_VAL_ARRAY) {                                                           \
            __emit_type_error(called_at, "array", __gl_value_get_type_name(val));                  \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_VECTOR(val)                                                                         \
    do {                                                                                           \
        if (val->type != GL_VAL_VECTOR) {                                                          \
            __emit_type_error(called_at, "vector", __gl_value_get_type_name(val));                 \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_CALLABLE(val)                                                                       \
    do {                                                                                           \
        if (val->type != GL_VAL_FUNCTION && val->type != GL_VAL_BUILTIN) {                         \
            __emit_type_error(called_at, "callable", __gl_value_get_type_name(val));               \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_TYPEID(val)                                                                         \
    do {                                                                                           \
        if (val->type != GL_VAL_TYPEID) {                                                          \
            __emit_type_error(called_at, "typeid", __gl_value_get_type_name(val));                 \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define EXPECT_FILE(val)                                                                           \
    do {                                                                                           \
        if (val->type != GL_VAL_FILE) {                                                            \
            __emit_type_error(called_at, "file", __gl_value_get_type_name(val));                   \
            return NULL;                                                                           \
        }                                                                                          \
    } while (0)

#define NTH_ARG(n) args->data[n]

#define NTH_VAL(n) NTH_ARG(n)->val

#define NTH_LOC(n) NTH_ARG(n)->location

#define ARGC args->size

// Shared argument validation and diagnostic helpers.
static void __emit_type_error(gl_location_t location, const char *expected, const char *got) {
    const char *fmt = "Argument type mismatch. Expected %s, got %s";
    char buf[1024] = "";

    snprintf(buf, sizeof(buf), fmt, expected, got);

    gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}

static void __emit_index_error(gl_location_t location, int64_t index, size_t size) {
    const char *fmt = "Index out of bounds: %lld (size %zu)";
    char buf[256] = "";

    snprintf(buf, sizeof(buf), fmt, (long long)index, size);

    gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}

static void __emit_empty_vector_error(gl_location_t location) {
    char buf[128] = "Cannot pop from empty vector";
    gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, location, buf);
    gl_diagnostic_report_error(err);
}

static const char *__gl_value_get_type_name(gl_value_t *val) {
    switch (val->type) {
    case GL_VAL_INT:
        return "int";
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
    case GL_VAL_NIL:
        return "nil";
    case GL_VAL_ARRAY:
        return "array";
    case GL_VAL_VECTOR:
        return "vector";
    case GL_VAL_FILE:
        return "file";
    case GL_VAL_ENV:
        assert(0 && "UNREACHABLE");
    }
}

GL_BUILTIN(exit) {
    int exit_code = 0;

    EXPECT_RVARGS(0, 1);

    if (ARGC == 1) {
        gl_value_t *val = NTH_VAL(0);
        EXPECT_INT(val);
        exit_code = gl_value_get_int(val);
    }

    exit(exit_code);
}

GL_BUILTIN(car) {
    EXPECT_ARGS(1);

    gl_value_t *list = NTH_VAL(0);

    EXPECT_LIST(list);

    gl_value_cons_t *cons = gl_value_get_cons(list);

    return cons->car;
}

GL_BUILTIN(cdr) {
    EXPECT_ARGS(1);

    gl_value_t *list = NTH_VAL(0);

    EXPECT_LIST(list);

    gl_value_cons_t *cons = gl_value_get_cons(list);

    return cons->cdr;
}

GL_BUILTIN(cons) {
    EXPECT_ARGS(2);

    return gl_value_make_cons_raw(NTH_VAL(0), NTH_VAL(1));
}

// Arithmetic promotes to float as soon as one operand is a float.
GL_BUILTIN(add) {
    if (ARGC == 0) {
        return gl_value_make_int(0);
    }

    int is_float = 0;
    int64_t isum = 0;
    long double fsum = 0;
    for (size_t i = 0; i < ARGC; ++i) {
        gl_value_t *current = NTH_VAL(i);
        EXPECT_NUMBER(current);
        if (!is_float) {
            if (current->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum + gl_value_get_float(current);
            } else {
                isum += gl_value_get_int(current);
            }
        } else {
            fsum += gl_value_get_float(current);
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(sub) {
    if (ARGC == 0) {
        return gl_value_make_int(0);
    }

    gl_value_t *first = NTH_VAL(0);
    EXPECT_NUMBER(first);

    int is_float = (first->type == GL_VAL_FLOAT);
    int64_t isum = is_float ? 0 : gl_value_get_int(first);
    long double fsum = is_float ? gl_value_get_float(first) : 0;

    // Unary operator handling
    if (ARGC == 1) {
        if (!is_float) {
            return gl_value_make_int(-isum);
        } else {
            return gl_value_make_float(-fsum);
        }
    }
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *current = NTH_VAL(i);
        EXPECT_NUMBER(current);
        if (!is_float) {
            if (current->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum - gl_value_get_float(current);
            } else {
                isum -= gl_value_get_int(current);
            }
        } else {
            fsum -= gl_value_get_float(current);
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(mul) {
    if (ARGC == 0) {
        return gl_value_make_int(1);
    }

    gl_value_t *first = NTH_VAL(0);
    EXPECT_NUMBER(first);

    int is_float = (first->type == GL_VAL_FLOAT);
    int64_t isum = is_float ? 0 : gl_value_get_int(first);
    long double fsum = is_float ? gl_value_get_float(first) : 0;

    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *current = NTH_VAL(i);
        EXPECT_NUMBER(current);
        if (!is_float) {
            if (current->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum * gl_value_get_float(current);
            } else {
                isum *= gl_value_get_int(current);
            }
        } else {
            fsum *= gl_value_get_float(current);
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(div) {
    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    EXPECT_NUMBER(first);

    int is_float = (first->type == GL_VAL_FLOAT);
    int64_t isum = is_float ? 0 : gl_value_get_int(first);
    long double fsum = is_float ? gl_value_get_float(first) : 0;

    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *current = NTH_VAL(i);
        EXPECT_NUMBER(current);

        if (current->type == GL_VAL_INT && gl_value_get_int(current) == 0) {
            gl_diagnostic_report_error(
                gl_make_error(GL_DIVISION_BY_ZERO, GL_ERROR, called_at, "Cannot divide by 0"));
            return NULL;
        }

        if (!is_float) {
            if (current->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum / gl_value_get_float(current);
            } else {
                isum /= gl_value_get_int(current);
            }
        } else {
            fsum /= gl_value_get_float(current);
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(mod) {
    EXPECT_ARGS(2);

    gl_value_t *lhs = NTH_VAL(0);
    gl_value_t *rhs = NTH_VAL(1);

    EXPECT_INT(lhs);
    EXPECT_INT(rhs);

    if (gl_value_get_int(rhs) == 0) {
        gl_diagnostic_report_error(
            gl_make_error(GL_DIVISION_BY_ZERO, GL_ERROR, called_at, "Cannot take modulus by 0"));
        return NULL;
    }

    return gl_value_make_int(gl_value_get_int(lhs) % gl_value_get_int(rhs));
}

GL_BUILTIN(eq) {
    if (ARGC == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *second = NTH_VAL(i);

        if (gl_value_compare(first, second) != 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(gt) {
    if (ARGC == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *second = NTH_VAL(i);

        if (gl_value_compare(first, second) <= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(lt) {
    if (ARGC == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *second = NTH_VAL(i);

        if (gl_value_compare(first, second) >= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(ge) {
    if (ARGC == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *second = NTH_VAL(i);

        if (gl_value_compare(first, second) < 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(le) {
    if (ARGC == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = NTH_VAL(0);
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_t *second = NTH_VAL(i);

        if (gl_value_compare(first, second) > 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(typeof) {
    EXPECT_ARGS(1);

    return gl_value_make_typeid(NTH_VAL(0)->type);
}

GL_BUILTIN(int_con) {
    EXPECT_ARGS(1);
    gl_value_t *val = NTH_VAL(0);

    EXPECT_NUMBER(val);

    if (val->type == GL_VAL_INT) {
        return val;
    }
    return gl_value_make_int((int64_t)gl_value_get_float(val));
}

GL_BUILTIN(float_con) {
    EXPECT_ARGS(1);
    gl_value_t *val = NTH_VAL(0);

    EXPECT_NUMBER(val);

    if (val->type == GL_VAL_FLOAT) {
        return val;
    }
    return gl_value_make_float((long double)gl_value_get_int(val));
}

GL_BUILTIN(list_con) {
    if (ARGC == 0) {
        return gl_value_make_nil();
    }

    gl_value_cons_t *list = malloc(sizeof(gl_value_cons_t));

    gl_value_cons_t *current = list;
    for (size_t i = 0; i < ARGC; ++i) {
        current->car = NTH_VAL(i);
        if (i == ARGC - 1) {
            current->cdr = gl_value_make_nil();
        } else {
            current->cdr = malloc(sizeof(gl_value_t));
            current->cdr->type = GL_VAL_CONS;
            current->cdr->val = malloc(sizeof(gl_value_cons_t));
            gl_register_value(current->cdr);
            current = (gl_value_cons_t *)current->cdr->val;
        }
    }
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = list;
    gl_register_value(res);
    return res;
}

GL_BUILTIN(array_con) {
    gl_value_array_t *arr = malloc(sizeof(gl_value_array_t));
    gl_value_array_init(arr, ARGC, NULL);
    for (size_t i = 0; i < ARGC; ++i) {
        arr->data[i] = NTH_VAL(i);
    }
    return gl_value_make_array_from(arr);
}

GL_BUILTIN(vector_con) {
    gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
    gl_value_vector_init(vec, NULL);
    for (size_t i = 0; i < ARGC; ++i) {
        gl_value_vector_push_back(vec, NTH_VAL(i));
    }
    return gl_value_make_vector_from(vec);
}

GL_BUILTIN(len) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    if (val->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = val->val;
        return gl_value_make_int((int64_t)arr->size);
    } else if (val->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = val->val;
        return gl_value_make_int((int64_t)vec->size);
    } else if (val->type == GL_VAL_STRING) {
        return gl_value_make_int((int64_t)strlen((char *)val->val));
    } else if (val->type == GL_VAL_CONS) {
        int64_t count = 0;
        gl_value_t *cur = val;
        while (cur && cur->type == GL_VAL_CONS) {
            count++;
            gl_value_cons_t *cons = gl_value_get_cons(cur);
            cur = cons->cdr;
        }
        return gl_value_make_int(count);
    } else if (val->type == GL_VAL_NIL) {
        return gl_value_make_int(0);
    } else {
        __emit_type_error(NTH_LOC(0), "sequence", __gl_value_get_type_name(val));
        return NULL;
    }
}

// get accepts both (coll index) and (index coll).
GL_BUILTIN(get) {
    EXPECT_ARGS(2);

    gl_function_arg_t *arg_coll = NTH_ARG(0);
    gl_function_arg_t *arg_idx = NTH_ARG(1);
    if (arg_coll->val->type == GL_VAL_INT &&
        (arg_idx->val->type == GL_VAL_ARRAY || arg_idx->val->type == GL_VAL_VECTOR ||
         arg_idx->val->type == GL_VAL_STRING || arg_idx->val->type == GL_VAL_CONS)) {
        arg_coll = NTH_ARG(1);
        arg_idx = NTH_ARG(0);
    }

    EXPECT_INT(arg_idx->val);

    int64_t idx = gl_value_get_int(arg_idx->val);
    gl_value_t *coll = arg_coll->val;

    if (idx < 0) {
        __emit_index_error(arg_idx->location, idx, 0);
        return NULL;
    }

    if (coll->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = coll->val;
        if ((size_t)idx >= arr->size) {
            __emit_index_error(arg_idx->location, idx, arr->size);
            return NULL;
        }
        return arr->data[idx];
    } else if (coll->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = coll->val;
        if ((size_t)idx >= vec->size) {
            __emit_index_error(arg_idx->location, idx, vec->size);
            return NULL;
        }
        return *gl_value_vector_at(vec, (size_t)idx);
    } else if (coll->type == GL_VAL_STRING) {
        char *str = coll->val;
        size_t slen = strlen(str);
        if ((size_t)idx >= slen) {
            __emit_index_error(arg_idx->location, idx, slen);
            return NULL;
        }
        char ch_buf[2] = {str[idx], '\0'};
        return gl_value_make_string(ch_buf);
    } else if (coll->type == GL_VAL_CONS) {
        gl_value_t *cur = coll;
        for (int64_t i = 0; i < idx; ++i) {
            if (cur == NULL || cur->type != GL_VAL_CONS) {
                __emit_index_error(arg_idx->location, idx, (size_t)i);
                return NULL;
            }
            cur = gl_value_get_cons(cur)->cdr;
        }
        if (cur == NULL || cur->type != GL_VAL_CONS) {
            __emit_index_error(arg_idx->location, idx, (size_t)idx);
            return NULL;
        }
        return gl_value_get_cons(cur)->car;
    } else {
        __emit_type_error(arg_coll->location, "sequence", __gl_value_get_type_name(coll));
        return NULL;
    }
}

GL_BUILTIN(set_at) {
    EXPECT_ARGS(3);

    gl_value_t *coll = NTH_VAL(0);
    gl_value_t *idx_val = NTH_VAL(1);
    EXPECT_INT(idx_val);
    int64_t idx = gl_value_get_int(idx_val);
    gl_value_t *new_val = NTH_VAL(2);

    if (idx < 0) {
        __emit_index_error(NTH_LOC(1), idx, 0);
        return NULL;
    }

    if (coll->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = coll->val;
        if ((size_t)idx >= arr->size) {
            __emit_index_error(NTH_LOC(1), idx, arr->size);
            return NULL;
        }
        arr->data[idx] = new_val;
        return new_val;
    } else if (coll->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = coll->val;
        if ((size_t)idx >= vec->size) {
            __emit_index_error(NTH_LOC(1), idx, vec->size);
            return NULL;
        }
        *gl_value_vector_at(vec, (size_t)idx) = new_val;
        return new_val;
    } else {
        __emit_type_error(NTH_LOC(0), "array or vector", __gl_value_get_type_name(coll));
        return NULL;
    }
}

GL_BUILTIN(vector_push) {
    EXPECT_VARGS(2);

    gl_value_t *vec_val = NTH_VAL(0);
    EXPECT_VECTOR(vec_val);

    gl_value_vector_t *vec = vec_val->val;
    for (size_t i = 1; i < ARGC; ++i) {
        gl_value_vector_push_back(vec, NTH_VAL(i));
    }
    return vec_val;
}

GL_BUILTIN(vector_pop) {
    EXPECT_ARGS(1);

    gl_value_t *vec_val = NTH_VAL(0);
    EXPECT_VECTOR(vec_val);

    gl_value_vector_t *vec = vec_val->val;
    if (vec->size == 0) {
        __emit_empty_vector_error(NTH_LOC(0));
        return NULL;
    }

    return gl_value_vector_pop_back(vec);
}

// Concatenates every argument into a new string.
GL_BUILTIN(string_append) {
    EXPECT_VARGS(0);

    size_t total = 0;
    for (size_t i = 0; i < ARGC; ++i) {
        EXPECT_STRING(NTH_VAL(i));
        total += strlen(gl_value_get_string(NTH_VAL(i)));
    }

    char *buf = malloc(total + 1);
    buf[0] = '\0';
    for (size_t i = 0; i < ARGC; ++i) {
        strcat(buf, gl_value_get_string(NTH_VAL(i)));
    }

    gl_value_t *res = gl_value_make_string(buf);
    free(buf);
    return res;
}

// Returns the bytes in [start, end); end defaults to the string length.
GL_BUILTIN(substring) {
    EXPECT_RVARGS(2, 3);

    gl_value_t *str_val = NTH_VAL(0);
    EXPECT_STRING(str_val);

    const char *str = gl_value_get_string(str_val);
    int64_t len = (int64_t)strlen(str);

    EXPECT_INT(NTH_VAL(1));
    int64_t start = gl_value_get_int(NTH_VAL(1));
    int64_t end = len;

    if (ARGC == 3) {
        EXPECT_INT(NTH_VAL(2));
        end = gl_value_get_int(NTH_VAL(2));
    }

    if (start < 0 || start > len) {
        __emit_index_error(NTH_LOC(1), start, (size_t)len);
        return NULL;
    }
    if (end < start || end > len) {
        __emit_index_error(NTH_LOC(2), end, (size_t)len);
        return NULL;
    }

    size_t size = (size_t)(end - start);
    char *buf = malloc(size + 1);
    memcpy(buf, str + start, size);
    buf[size] = '\0';

    gl_value_t *res = gl_value_make_string(buf);
    free(buf);
    return res;
}

GL_BUILTIN(string_upcase) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_STRING(val);

    char *buf = strdup(gl_value_get_string(val));
    for (char *p = buf; *p != '\0'; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }

    gl_value_t *res = gl_value_make_string(buf);
    free(buf);
    return res;
}

GL_BUILTIN(string_downcase) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_STRING(val);

    char *buf = strdup(gl_value_get_string(val));
    for (char *p = buf; *p != '\0'; ++p) {
        *p = (char)tolower((unsigned char)*p);
    }

    gl_value_t *res = gl_value_make_string(buf);
    free(buf);
    return res;
}

// The final argument is a collection whose elements are appended to the call.
GL_BUILTIN(apply) {
    EXPECT_VARGS(2);
    gl_value_t *fn = NTH_VAL(0);
    EXPECT_CALLABLE(fn);

    gl_value_t *last_arg = NTH_VAL(ARGC - 1);
    if (last_arg->type != GL_VAL_CONS && last_arg->type != GL_VAL_NIL &&
        last_arg->type != GL_VAL_ARRAY && last_arg->type != GL_VAL_VECTOR) {
        __emit_type_error(NTH_LOC(ARGC - 1), "list, array, or vector",
                          __gl_value_get_type_name(last_arg));
        return NULL;
    }

    size_t prefix_count = ARGC - 2;
    size_t rest_count = 0;
    if (last_arg->type == GL_VAL_ARRAY) {
        rest_count = gl_value_get_array(last_arg)->size;
    } else if (last_arg->type == GL_VAL_VECTOR) {
        rest_count = gl_value_get_vector(last_arg)->size;
    } else if (last_arg->type == GL_VAL_CONS) {
        gl_value_t *cur = last_arg;
        while (cur && cur->type == GL_VAL_CONS) {
            rest_count++;
            cur = gl_value_get_cons(cur)->cdr;
        }
    }

    size_t total_args = prefix_count + rest_count;
    gl_function_args_t apply_args;
    gl_function_args_init(&apply_args, total_args, gl_function_arg_free);

    for (size_t i = 0; i < prefix_count; ++i) {
        apply_args.data[i] = malloc(sizeof(gl_function_arg_t));
        apply_args.data[i]->location = NTH_LOC(i + 1);
        apply_args.data[i]->val = NTH_VAL(i + 1);
    }

    if (last_arg->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = gl_value_get_array(last_arg);
        for (size_t i = 0; i < rest_count; ++i) {
            apply_args.data[prefix_count + i] = malloc(sizeof(gl_function_arg_t));
            apply_args.data[prefix_count + i]->location = called_at;
            apply_args.data[prefix_count + i]->val = arr->data[i];
        }
    } else if (last_arg->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = gl_value_get_vector(last_arg);
        for (size_t i = 0; i < rest_count; ++i) {
            apply_args.data[prefix_count + i] = malloc(sizeof(gl_function_arg_t));
            apply_args.data[prefix_count + i]->location = called_at;
            apply_args.data[prefix_count + i]->val = *gl_value_vector_at(vec, i);
        }
    } else if (last_arg->type == GL_VAL_CONS) {
        gl_value_t *cur = last_arg;
        for (size_t i = 0; i < rest_count; ++i) {
            apply_args.data[prefix_count + i] = malloc(sizeof(gl_function_arg_t));
            apply_args.data[prefix_count + i]->location = called_at;
            apply_args.data[prefix_count + i]->val = gl_value_get_cons(cur)->car;
            cur = gl_value_get_cons(cur)->cdr;
        }
    }

    gl_value_t *res = gl_call_function(called_at, fn, &apply_args);
    gl_function_args_destroy(&apply_args);
    return res;
}

static char *__get_directory(const char *path) {
    if (!path || path[0] == '\0' || path[0] == '<') {
        return strdup(".");
    }
    const char *last_slash = strrchr(path, '/');
#ifdef _WIN32
    const char *last_backslash = strrchr(path, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
#endif
    if (!last_slash) {
        return strdup(".");
    }
    if (last_slash == path) {
        return strdup("/");
    }
    size_t len = last_slash - path;
    char *dir = malloc(len + 1);
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static char *__gl_realpath(const char *path) {
#ifdef _WIN32
    return _fullpath(NULL, path, 0);
#else
    return realpath(path, NULL);
#endif
}

static int __is_absolute_path(const char *path) {
    if (!path) {
        return 0;
    }
    if (path[0] == '/') {
        return 1;
    }
#ifdef _WIN32
    if ((isalpha((unsigned char)path[0]) && path[1] == ':') || path[0] == '\\') {
        return 1;
    }
#endif
    return 0;
}

gl_value_t *gl_load_file(const char *filepath, const char *caller_unit) {
    if (!filepath) {
        return NULL;
    }

    char *target_path = NULL;

    if (__is_absolute_path(filepath)) {
        target_path = __gl_realpath(filepath);
        if (!target_path) {
            target_path = strdup(filepath);
        }
    } else {
        char *dir = __get_directory(caller_unit);
        size_t combined_len = strlen(dir) + 1 + strlen(filepath) + 1;
        char *combined = malloc(combined_len);
        snprintf(combined, combined_len, "%s/%s", dir, filepath);
        free(dir);

        target_path = __gl_realpath(combined);
        if (!target_path) {
            target_path = combined;
        } else {
            free(combined);
        }
    }

    FILE *file = fopen(target_path, "r");
    if (!file) {
        free(target_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    if (filesize < 0) {
        fclose(file);
        free(target_path);
        return NULL;
    }

    char *buf = malloc(filesize + 1);
    if (!buf) {
        fclose(file);
        free(target_path);
        return NULL;
    }

    size_t bytesread = fread(buf, sizeof(char), filesize, file);
    buf[bytesread] = '\0';
    fclose(file);

    gl_register_source(buf, target_path);

    gl_call_stack_push_env(gl_global_env, NULL, "load");
    gl_parser_t parser = gl_make_parser(buf, target_path);
    gl_value_t *res = gl_parse_and_eval(&parser, 1);
    gl_parser_destroy(&parser);
    gl_call_stack_pop_env();

    if (!res) {
        res = gl_value_make_bool(1);
    }
    return res;
}

GL_BUILTIN(load) {
    EXPECT_ARGS(1);

    gl_value_t *arg = NTH_VAL(0);
    EXPECT_STRING(arg);

    const char *filepath = gl_value_get_string(arg);
    gl_value_t *res = gl_load_file(filepath, called_at.unit_name);
    if (!res) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Cannot load file '%s'", filepath);
        gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf);
        gl_diagnostic_report_error(err);
        return NULL;
    }
    return res;
}

int gl_load_stdlib(void) {
    const char *env_path = getenv("GLISP_STD_PATH");
    if (!env_path || env_path[0] == '\0') {
        env_path = getenv("GLISP_STD");
    }

    if (env_path && env_path[0] != '\0') {
        struct stat st;
        if (stat(env_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                size_t env_len = strlen(env_path);
                int has_slash = (env_len > 0 &&
                                 (env_path[env_len - 1] == '/' || env_path[env_len - 1] == '\\'));
                size_t len = env_len + (has_slash ? 0 : 1) + strlen("std.lisp") + 1;
                char *target = malloc(len);
                snprintf(target, len, "%s%sstd.lisp", env_path, has_slash ? "" : "/");
                gl_value_t *res = gl_load_file(target, NULL);
                free(target);
                return res ? 0 : -1;
            } else {
                gl_value_t *res = gl_load_file(env_path, NULL);
                return res ? 0 : -1;
            }
        } else {
            fprintf(stderr, "Error: standard library path '%s' not found\n", env_path);
            return -1;
        }
    }

    struct stat st;
    if (stat("std/std.lisp", &st) == 0 && !S_ISDIR(st.st_mode)) {
        gl_value_t *res = gl_load_file("std/std.lisp", NULL);
        return res ? 0 : -1;
    }

#ifdef GLISP_SOURCE_DIR
    if (strlen(GLISP_SOURCE_DIR) > 0) {
        const char *src_std = GLISP_SOURCE_DIR "/std/std.lisp";
        if (stat(src_std, &st) == 0 && !S_ISDIR(st.st_mode)) {
            gl_value_t *res = gl_load_file(src_std, NULL);
            return res ? 0 : -1;
        }
    }
#endif

    return -1;
}

GL_BUILTIN(assert) {
    EXPECT_RVARGS(1, 2);

    gl_value_t *cond = NTH_VAL(0);
    char *msg = NULL;
    if (ARGC == 2) {
        gl_value_t *msg_val = NTH_VAL(1);
        EXPECT_STRING(msg_val);
        msg = gl_value_get_string(msg_val);
    }

    if (!gl_value_get_bool(cond)) {
        gl_diagnostic_report_error(gl_make_error(GL_ASSERTION_FAIL, GL_FATAL, called_at, msg));
        return NULL;
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(sqrt) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(sqrt(gl_value_get_float(val)));
}

GL_BUILTIN(fma) {
    EXPECT_ARGS(3);

    gl_value_t *x = NTH_VAL(0);
    gl_value_t *y = NTH_VAL(1);
    gl_value_t *z = NTH_VAL(2);

    EXPECT_NUMBER(x);
    EXPECT_NUMBER(y);
    EXPECT_NUMBER(z);

    long double fx = gl_value_get_float(x);
    long double fy = gl_value_get_float(y);
    long double fz = gl_value_get_float(z);

    return gl_value_make_float(fma(fx, fy, fz));
}

GL_BUILTIN(copysign) {
    EXPECT_ARGS(2);

    gl_value_t *x = NTH_VAL(0);
    gl_value_t *y = NTH_VAL(1);

    EXPECT_NUMBER(x);
    EXPECT_NUMBER(y);

    long double fx = gl_value_get_float(x);
    long double fy = gl_value_get_float(y);

    return gl_value_make_float(copysign(fx, fy));
}

GL_BUILTIN(exp) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(exp(gl_value_get_float(val)));
}

GL_BUILTIN(exp2) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(exp2(gl_value_get_float(val)));
}

GL_BUILTIN(log) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(log(gl_value_get_float(val)));
}

GL_BUILTIN(log2) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(log2(gl_value_get_float(val)));
}

GL_BUILTIN(floor) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(floor(gl_value_get_float(val)));
}

GL_BUILTIN(ceil) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(ceil(gl_value_get_float(val)));
}

GL_BUILTIN(round) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(round(gl_value_get_float(val)));
}

GL_BUILTIN(sin) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(sin(gl_value_get_float(val)));
}

GL_BUILTIN(cos) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);
    EXPECT_NUMBER(val);

    return gl_value_make_float(cos(gl_value_get_float(val)));
}

GL_BUILTIN(atan2) {
    EXPECT_ARGS(2);

    gl_value_t *y = NTH_VAL(0);
    gl_value_t *x = NTH_VAL(1);

    EXPECT_NUMBER(y);
    EXPECT_NUMBER(x);

    return gl_value_make_float(atan2(gl_value_get_float(y), gl_value_get_float(x)));
}

GL_BUILTIN(pow) {
    EXPECT_ARGS(2);

    gl_value_t *x = NTH_VAL(0);
    gl_value_t *power = NTH_VAL(1);

    EXPECT_NUMBER(x);
    EXPECT_NUMBER(power);

    if (gl_value_get_float(x) == 0.0l && gl_value_get_float(power) == 0.0l) {
        gl_error_t *err = gl_make_error(GL_DIVISION_BY_ZERO, GL_ERROR, called_at,
                                        "Cannot raise 0 to the power of 0");
        gl_diagnostic_report_error(err);
        return NULL;
    }

    return gl_value_make_float(pow(gl_value_get_float(x), gl_value_get_float(power)));
}

static gl_file_mode_t __gl_parse_file_mode(const char *mode) {
    if (!mode) {
        return 0;
    }

    char *current = (char *)mode;

    gl_file_mode_t res = 0;
    while (*current != '\0') {
        switch (*current) {
        case 'r':
            res |= GL_FILE_READ;
            break;
        case 'w':
            res |= GL_FILE_WRITE;
            break;
        case 'a':
            res |= GL_FILE_APPEND;
            break;
        case '+':
            res |= (GL_FILE_READ | GL_FILE_WRITE);
            break;
        case 'b':
            break;
        default:
            return 0;
        }
        current += 1;
    }
    return res;
}

// Maps a permission bitmask to the corresponding fopen mode string.
static const char *__gl_file_mode_to_fopen(gl_file_mode_t mode) {
    int can_read = (mode & GL_FILE_READ) != 0;
    int can_write = (mode & GL_FILE_WRITE) != 0;

    if (mode & GL_FILE_APPEND) {
        return can_read ? "a+" : "a";
    }
    if (can_read && can_write) {
        return "r+";
    }
    if (can_write) {
        return "w";
    }
    return "r";
}

GL_BUILTIN(open) {
    EXPECT_ARGS(2);

    gl_value_t *filename = NTH_VAL(0);
    gl_value_t *mode = NTH_VAL(1);

    EXPECT_STRING(filename);

    char *filename_str = gl_value_get_string(filename);
    const char *mode_str = NULL;
    gl_file_mode_t file_mode = 0;

    if (mode->type == GL_VAL_STRING) {
        mode_str = gl_value_get_string(mode);
        file_mode = __gl_parse_file_mode(mode_str);

        if (file_mode == 0) {
            gl_diagnostic_report_error(
                gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, called_at,
                              "Wrong file mode string: expected \"r\", \"w\" or \"a\", optionally "
                              "followed by \"+\" and/or \"b\""));
            return NULL;
        }
    } else if (mode->type == GL_VAL_INT) {
        int64_t raw_mode = gl_value_get_int(mode);
        int64_t valid_bits = GL_FILE_READ | GL_FILE_WRITE | GL_FILE_APPEND;

        if (raw_mode <= 0 || (raw_mode & ~valid_bits) != 0) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf),
                     "Wrong file mode: expected a combination of read (%d), write (%d) and "
                     "append (%d)",
                     GL_FILE_READ, GL_FILE_WRITE, GL_FILE_APPEND);
            gl_diagnostic_report_error(
                gl_make_error(GL_SYNTAX_ERROR, GL_ERROR, called_at, err_buf));
            return NULL;
        }

        file_mode = (gl_file_mode_t)raw_mode;
        mode_str = __gl_file_mode_to_fopen(file_mode);
    } else {
        __emit_type_error(called_at, "string or int", __gl_value_get_type_name(mode));
        return NULL;
    }

    FILE *file = fopen(filename_str, mode_str);
    if (!file) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Cannot open file '%s': %s", filename_str,
                 strerror(errno));
        gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf));
        return NULL;
    }

    return gl_value_make_file(file, filename_str, 1, file_mode);
}

GL_BUILTIN(close) {
    EXPECT_ARGS(1);

    gl_value_t *file = NTH_VAL(0);

    EXPECT_FILE(file);

    gl_file_t *f = gl_value_get_file(file);

    if (f->handle == stdin || f->handle == stdout || f->handle == stderr) {
        return gl_value_make_bool(0);
    }

    if (!f->is_open) {
        return gl_value_make_bool(0);
    }

    int status = fclose(f->handle);

    if (status != 0) {
        return gl_value_make_bool(0);
    }

    f->is_open = 0;
    f->handle = NULL;

    return gl_value_make_bool(1);
}

GL_BUILTIN(write_str) {
    EXPECT_RVARGS(1, 2);

    gl_value_t *str = NTH_VAL(0);

    FILE *dest = stdout;

    if (ARGC == 2) {
        gl_value_t *file = NTH_VAL(1);

        EXPECT_FILE(file);

        gl_file_t *f = gl_value_get_file(file);

        if (!f->is_open || !f->handle) {
            gl_diagnostic_report_error(
                gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, "Cannot write to a closed file"));
            return NULL;
        }

        if (!(f->mode & (GL_FILE_WRITE | GL_FILE_APPEND))) {
            gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at,
                                                     "File is not opened for writing"));
            return NULL;
        }

        dest = f->handle;
    }

    const char *text = gl_value_get_string(str);
    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, dest);

    if (written != len || ferror(dest)) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Failed to write to file: %s",
                 errno != 0 ? strerror(errno) : "I/O error");
        gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf));
        return NULL;
    }

    return gl_value_make_int((int64_t)written);
}

GL_BUILTIN(to_str) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);

    if (val->type == GL_VAL_STRING) {
        return gl_value_copy(val);
    }

    char *str = gl_value_to_string(val);
    gl_value_t *res = gl_value_make_string(str);
    free(str);
    return res;
}

// Reads one line, strips LF or CRLF, and returns nil at end of file.
#ifdef _WIN32
static long long __gl_getline(char **lineptr, size_t *n, FILE *stream) {
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) {
            return -1;
        }
    }

    size_t len = 0;
    int ch = EOF;
    while ((ch = fgetc(stream)) != EOF) {
        if (len + 1 >= *n) {
            size_t new_cap = *n * 2;
            char *grown = realloc(*lineptr, new_cap);
            if (grown == NULL) {
                return -1;
            }
            *lineptr = grown;
            *n = new_cap;
        }
        (*lineptr)[len++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (len == 0 && ch == EOF) {
        return -1;
    }

    (*lineptr)[len] = '\0';
    return (long long)len;
}

#define GL_GETLINE __gl_getline
#define GL_SSIZE_T long long
#else
#define GL_GETLINE getline
#define GL_SSIZE_T ssize_t
#endif

GL_BUILTIN(read_line) {
    EXPECT_RVARGS(0, 1);

    FILE *stream = stdin;
    if (ARGC == 1) {
        gl_value_t *f_val = NTH_VAL(0);
        EXPECT_FILE(f_val);
        gl_file_t *f = gl_value_get_file(f_val);
        if (!f->is_open || !f->handle) {
            gl_diagnostic_report_error(
                gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, "Cannot read from closed file"));
            return NULL;
        }
        if (!(f->mode & GL_FILE_READ)) {
            gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at,
                                                     "File is not opened for reading"));
            return NULL;
        }
        stream = f->handle;
    }

    char *line = NULL;
    size_t cap = 0;
    GL_SSIZE_T nread = GL_GETLINE(&line, &cap, stream);

    if (nread == -1) {
        free(line);
        return gl_value_make_nil(); // EOF
    }

    // Cutting newline character if present
    if (nread > 0 && line[nread - 1] == '\n') {
        line[nread - 1] = '\0';
        if (nread > 1 && line[nread - 2] == '\r') {
            line[nread - 2] = '\0'; // CRLF
        }
    }

    gl_value_t *res = gl_value_make_string(line);
    free(line);
    return res;
}

GL_BUILTIN(open_p) {
    EXPECT_ARGS(1);

    gl_value_t *file = NTH_VAL(0);

    EXPECT_FILE(file);

    return gl_value_make_bool(gl_value_get_file(file)->is_open);
}

GL_BUILTIN(file_mode) {
    EXPECT_ARGS(1);

    gl_value_t *file = NTH_VAL(0);

    EXPECT_FILE(file);

    return gl_value_make_int((int64_t)gl_value_get_file(file)->mode);
}

GL_BUILTIN(eof_p) {
    EXPECT_ARGS(1);

    gl_value_t *file = NTH_VAL(0);

    EXPECT_FILE(file);

    gl_file_t *f = gl_value_get_file(file);

    if (!f->is_open || !f->handle) {
        gl_diagnostic_report_error(
            gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, "Cannot check EOF of a closed file"));
        return NULL;
    }

    return gl_value_make_bool(feof(f->handle));
}

GL_BUILTIN(flush) {
    EXPECT_RVARGS(0, 1);

    FILE *stream = stdout;

    if (ARGC == 1) {
        gl_value_t *file = NTH_VAL(0);

        EXPECT_FILE(file);

        gl_file_t *f = gl_value_get_file(file);

        if (!f->is_open || !f->handle) {
            gl_diagnostic_report_error(
                gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, "Cannot flush a closed file"));
            return NULL;
        }

        stream = f->handle;
    }

    if (fflush(stream) != 0) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Failed to flush file: %s",
                 errno != 0 ? strerror(errno) : "I/O error");
        gl_diagnostic_report_error(gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf));
        return NULL;
    }

    return gl_value_make_nil();
}

GL_BUILTIN(logand) {
    EXPECT_ARGS(2);

    gl_value_t *lhs = NTH_VAL(0);
    gl_value_t *rhs = NTH_VAL(1);

    EXPECT_INT(lhs);
    EXPECT_INT(rhs);

    return gl_value_make_int(gl_value_get_int(lhs) & gl_value_get_int(rhs));
}

GL_BUILTIN(logior) {
    EXPECT_ARGS(2);

    gl_value_t *lhs = NTH_VAL(0);
    gl_value_t *rhs = NTH_VAL(1);

    EXPECT_INT(lhs);
    EXPECT_INT(rhs);

    return gl_value_make_int(gl_value_get_int(lhs) | gl_value_get_int(rhs));
}

GL_BUILTIN(logxor) {
    EXPECT_ARGS(2);

    gl_value_t *lhs = NTH_VAL(0);
    gl_value_t *rhs = NTH_VAL(1);

    EXPECT_INT(lhs);
    EXPECT_INT(rhs);

    return gl_value_make_int(gl_value_get_int(lhs) ^ gl_value_get_int(rhs));
}

GL_BUILTIN(lognot) {
    EXPECT_ARGS(1);

    gl_value_t *val = NTH_VAL(0);

    EXPECT_INT(val);

    return gl_value_make_int(~gl_value_get_int(val));
}
