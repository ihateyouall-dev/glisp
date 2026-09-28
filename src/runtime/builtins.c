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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MSC_VER
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
    case GL_VAL_ENV:
        assert(0 && "UNREACHABLE");
    }
}

GL_BUILTIN(exit) {
    assert(args);
    assert(args->data[0]->val);

    EXPECT_ARGS(1);

    if (args->data[0]->val->type != GL_VAL_INT) {
        __emit_type_error(args->data[0]->location, "int",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    exit(gl_value_get_int(args->data[0]->val));
}

GL_BUILTIN(print) {
    if (args->size == 0) {
        return gl_value_make_nil();
    }

    for (size_t i = 0; i < args->size; ++i) {
        gl_value_print(args->data[i]->val);
    }
    return gl_value_make_nil();
}

GL_BUILTIN(println) {
    if (args->size == 0) {
        printf("\n");
        return gl_value_make_nil();
    }
    gl_builtin_print(called_at, args);
    printf("\n");
    return gl_value_make_nil();
}

GL_BUILTIN(car) {
    EXPECT_ARGS(1);

    if (args->data[0]->val->type != GL_VAL_CONS) {
        __emit_type_error(called_at, "list", __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    gl_value_cons_t *cons = gl_value_get_cons(args->data[0]->val);

    return cons->car;
}

GL_BUILTIN(cdr) {
    EXPECT_ARGS(1);

    if (args->data[0]->val->type != GL_VAL_CONS) {
        __emit_type_error(called_at, "list", __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    gl_value_cons_t *cons = gl_value_get_cons(args->data[0]->val);

    return cons->cdr;
}

GL_BUILTIN(cons) {
    EXPECT_ARGS(2);

    return gl_value_make_cons_raw(args->data[0]->val, args->data[1]->val);
}

GL_BUILTIN(add) {
    if (args->size == 0) {
        return gl_value_make_int(0);
    }

    int is_float = 0;
    int64_t isum = 0;
    long double fsum = 0;
    for (size_t i = 0; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (!is_float) {
            if (current->val->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum + gl_value_get_float(current->val);
            } else {
                isum += gl_value_get_int(current->val);
            }
        } else {
            if (current->val->type == GL_VAL_INT) {
                fsum += (long double)gl_value_get_int(current->val);
            } else {
                fsum += gl_value_get_float(current->val);
            }
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(sub) {
    if (args->size == 0) {
        return gl_value_make_int(0);
    }

    int is_float = 0;
    int64_t isum = 0;
    long double fsum = 0;
    // Substraction goes from first arg
    if (args->data[0]->val->type != GL_VAL_INT && args->data[0]->val->type != GL_VAL_FLOAT) {
        __emit_type_error(args->data[0]->location, "number",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }
    if (args->data[0]->val->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]->val);
    } else {
        is_float = 1;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    // Unary operator handling
    if (args->size == 1) {
        if (!is_float) {
            return gl_value_make_int(-isum);
        } else {
            return gl_value_make_float(-fsum);
        }
    }
    for (size_t i = 1; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (!is_float) {
            if (current->val->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum - gl_value_get_float(current->val);
            } else {
                isum -= gl_value_get_int(current->val);
            }
        } else {
            if (current->val->type == GL_VAL_INT) {
                fsum -= (long double)gl_value_get_int(current->val);
            } else {
                fsum -= gl_value_get_float(current->val);
            }
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(mul) {
    if (args->size == 0) {
        return gl_value_make_int(0);
    }

    int is_float = 0;
    int64_t isum = 0;
    long double fsum = 0;
    if (args->data[0]->val->type != GL_VAL_INT && args->data[0]->val->type != GL_VAL_FLOAT) {
        __emit_type_error(args->data[0]->location, "number",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }
    if (args->data[0]->val->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]->val);
    } else {
        is_float = 1;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    for (size_t i = 1; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (!is_float) {
            if (current->val->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum * gl_value_get_float(current->val);
            } else {
                isum *= gl_value_get_int(current->val);
            }
        } else {
            if (current->val->type == GL_VAL_INT) {
                fsum *= (long double)gl_value_get_int(current->val);
            } else {
                fsum *= gl_value_get_float(current->val);
            }
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(div) {
    EXPECT_VARGS(1);

    int is_float = 0;
    int64_t isum = 0;
    long double fsum = 0;

    if (args->data[0]->val->type != GL_VAL_INT && args->data[0]->val->type != GL_VAL_FLOAT) {
        __emit_type_error(args->data[0]->location, "number",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }
    if (args->data[0]->val->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]->val);
    } else {
        is_float = 1;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    for (size_t i = 1; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (!is_float) {
            if (current->val->type == GL_VAL_FLOAT) {
                is_float = 1;
                fsum = (long double)isum / gl_value_get_float(current->val);
            } else {
                isum /= gl_value_get_int(current->val);
            }
        } else {
            if (current->val->type == GL_VAL_INT) {
                fsum /= (long double)gl_value_get_int(current->val);
            } else {
                fsum /= gl_value_get_float(current->val);
            }
        }
    }
    if (is_float) {
        return gl_value_make_float(fsum);
    }
    return gl_value_make_int(isum);
}

GL_BUILTIN(mod) {
    EXPECT_ARGS(2);

    gl_value_t *lhs = args->data[0]->val;
    gl_value_t *rhs = args->data[1]->val;

    if (args->data[0]->val->type != GL_VAL_INT) {
        __emit_type_error(args->data[0]->location, "int",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }
    if (args->data[1]->val->type != GL_VAL_INT) {
        __emit_type_error(args->data[0]->location, "int",
                          __gl_value_get_type_name(args->data[1]->val));
        return NULL;
    }

    return gl_value_make_int(gl_value_get_int(lhs) % gl_value_get_int(rhs));
}

GL_BUILTIN(eq) {
    if (args->size == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (gl_value_compare(first, second) != 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(gt) {
    if (args->size == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (gl_value_compare(first, second) <= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(lt) {
    if (args->size == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (gl_value_compare(first, second) >= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(ge) {
    if (args->size == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (gl_value_compare(first, second) < 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(le) {
    if (args->size == 1) {
        return gl_value_make_bool(1);
    }

    EXPECT_VARGS(1);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (gl_value_compare(first, second) > 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(not ) {
    EXPECT_ARGS(1);

    int val = gl_value_get_bool(args->data[0]->val);

    return gl_value_make_bool(!val);
}

GL_BUILTIN(and) {
    EXPECT_VARGS(2);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (!(gl_value_get_bool(first) && gl_value_get_bool(second))) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(or) {
    EXPECT_VARGS(2);

    gl_value_t *first = args->data[0]->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i]->val;

        if (!(gl_value_get_bool(first) || gl_value_get_bool(second))) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(typeof) {
    EXPECT_ARGS(1);

    return gl_value_make_typeid(args->data[0]->val->type);
}

GL_BUILTIN(int_con) {
    gl_function_arg_t *arg = args->data[0];
    gl_value_t *val = arg->val;

    if (val->type != GL_VAL_INT && val->type != GL_VAL_FLOAT) {
        __emit_type_error(arg->location, "number", __gl_value_get_type_name(val));
        return NULL;
    }

    if (val->type == GL_VAL_INT) {
        return val;
    } else if (val->type == GL_VAL_FLOAT) {
        return gl_value_make_int((int64_t)gl_value_get_float(val));
    } else {
        assert(0 && "Only types castable to INT is INT and FLOAT");
    }
}

GL_BUILTIN(float_con) {
    gl_function_arg_t *arg = args->data[0];
    gl_value_t *val = arg->val;

    if (val->type != GL_VAL_INT && val->type != GL_VAL_FLOAT) {
        __emit_type_error(arg->location, "number", __gl_value_get_type_name(val));
        return NULL;
    }

    if (val->type == GL_VAL_INT) {
        return gl_value_make_float((long double)gl_value_get_int(val));
    } else if (val->type == GL_VAL_FLOAT) {
        return val;
    } else {
        assert(0 && "Only types castable to FLOAT is FLOAT and INT");
    }
}

GL_BUILTIN(list_con) {
    if (args->size == 0) {
        return gl_value_make_nil();
    }

    gl_value_cons_t *list = malloc(sizeof(gl_value_cons_t));

    gl_value_cons_t *current = list;
    for (size_t i = 0; i < args->size; ++i) {
        current->car = args->data[i]->val;
        if (i == args->size - 1) {
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
    gl_value_array_init(arr, args->size, NULL);
    for (size_t i = 0; i < args->size; ++i) {
        arr->data[i] = args->data[i]->val;
    }
    return gl_value_make_array_from(arr);
}

GL_BUILTIN(vector_con) {
    gl_value_vector_t *vec = malloc(sizeof(gl_value_vector_t));
    gl_value_vector_init(vec, NULL);
    for (size_t i = 0; i < args->size; ++i) {
        gl_value_vector_push_back(vec, args->data[i]->val);
    }
    return gl_value_make_vector_from(vec);
}

GL_BUILTIN(len) {
    EXPECT_ARGS(1);

    gl_value_t *val = args->data[0]->val;
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
        __emit_type_error(args->data[0]->location, "sequence", __gl_value_get_type_name(val));
        return NULL;
    }
}

GL_BUILTIN(get) {
    EXPECT_ARGS(2);

    gl_function_arg_t *arg_coll = args->data[0];
    gl_function_arg_t *arg_idx = args->data[1];
    if (arg_coll->val->type == GL_VAL_INT &&
        (arg_idx->val->type == GL_VAL_ARRAY || arg_idx->val->type == GL_VAL_VECTOR ||
         arg_idx->val->type == GL_VAL_STRING || arg_idx->val->type == GL_VAL_CONS)) {
        arg_coll = args->data[1];
        arg_idx = args->data[0];
    }

    if (arg_idx->val->type != GL_VAL_INT) {
        __emit_type_error(arg_idx->location, "int", __gl_value_get_type_name(arg_idx->val));
        return NULL;
    }

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

    gl_value_t *coll = args->data[0]->val;
    if (args->data[1]->val->type != GL_VAL_INT) {
        __emit_type_error(args->data[1]->location, "int",
                          __gl_value_get_type_name(args->data[1]->val));
        return NULL;
    }
    int64_t idx = gl_value_get_int(args->data[1]->val);
    gl_value_t *new_val = args->data[2]->val;

    if (idx < 0) {
        __emit_index_error(args->data[1]->location, idx, 0);
        return NULL;
    }

    if (coll->type == GL_VAL_ARRAY) {
        gl_value_array_t *arr = coll->val;
        if ((size_t)idx >= arr->size) {
            __emit_index_error(args->data[1]->location, idx, arr->size);
            return NULL;
        }
        arr->data[idx] = new_val;
        return new_val;
    } else if (coll->type == GL_VAL_VECTOR) {
        gl_value_vector_t *vec = coll->val;
        if ((size_t)idx >= vec->size) {
            __emit_index_error(args->data[1]->location, idx, vec->size);
            return NULL;
        }
        *gl_value_vector_at(vec, (size_t)idx) = new_val;
        return new_val;
    } else {
        __emit_type_error(args->data[0]->location, "array or vector",
                          __gl_value_get_type_name(coll));
        return NULL;
    }
}

GL_BUILTIN(vector_push) {
    EXPECT_VARGS(2);

    if (args->data[0]->val->type != GL_VAL_VECTOR) {
        __emit_type_error(args->data[0]->location, "vector",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    gl_value_vector_t *vec = args->data[0]->val->val;
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_vector_push_back(vec, args->data[i]->val);
    }
    return args->data[0]->val;
}

GL_BUILTIN(vector_pop) {
    EXPECT_ARGS(1);

    if (args->data[0]->val->type != GL_VAL_VECTOR) {
        __emit_type_error(args->data[0]->location, "vector",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    gl_value_vector_t *vec = args->data[0]->val->val;
    if (vec->size == 0) {
        __emit_empty_vector_error(args->data[0]->location);
        return NULL;
    }

    return gl_value_vector_pop_back(vec);
}

GL_BUILTIN(apply) {
    EXPECT_VARGS(2);
    gl_value_t *fn = args->data[0]->val;
    if (fn->type != GL_VAL_FUNCTION && fn->type != GL_VAL_BUILTIN) {
        __emit_type_error(args->data[0]->location, "callable", __gl_value_get_type_name(fn));
        return NULL;
    }

    gl_value_t *last_arg = args->data[args->size - 1]->val;
    if (last_arg->type != GL_VAL_CONS && last_arg->type != GL_VAL_NIL &&
        last_arg->type != GL_VAL_ARRAY && last_arg->type != GL_VAL_VECTOR) {
        __emit_type_error(args->data[args->size - 1]->location, "list, array, or vector",
                          __gl_value_get_type_name(last_arg));
        return NULL;
    }

    size_t prefix_count = args->size - 2;
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
        apply_args.data[i]->location = args->data[i + 1]->location;
        apply_args.data[i]->val = args->data[i + 1]->val;
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

GL_BUILTIN(load) {
    EXPECT_ARGS(1);
    if (args->data[0]->val->type != GL_VAL_STRING) {
        __emit_type_error(args->data[0]->location, "string",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }

    const char *filepath = args->data[0]->val->val;
    char *target_path = NULL;

    if (__is_absolute_path(filepath)) {
        target_path = realpath(filepath, NULL);
        if (!target_path) {
            target_path = strdup(filepath);
        }
    } else {
        char *dir = __get_directory(called_at.unit_name);
        size_t combined_len = strlen(dir) + 1 + strlen(filepath) + 1;
        char *combined = malloc(combined_len);
        snprintf(combined, combined_len, "%s/%s", dir, filepath);
        free(dir);

        target_path = realpath(combined, NULL);
        if (!target_path) {
            target_path = combined;
        } else {
            free(combined);
        }
    }

    FILE *file = fopen(target_path, "r");
    if (!file) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Cannot load file '%s'", filepath);
        gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf);
        gl_diagnostic_report_error(err);
        free(target_path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    if (filesize < 0) {
        fclose(file);
        free(target_path);
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "Cannot read file '%s'", filepath);
        gl_error_t *err = gl_make_error(GL_TYPE_ERROR, GL_ERROR, called_at, err_buf);
        gl_diagnostic_report_error(err);
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
    gl_value_t *res = gl_parse_and_eval(&parser);
    gl_parser_destroy(&parser);
    gl_call_stack_pop_env();

    if (!res) {
        res = gl_value_make_bool(1);
    }
    return res;
}
