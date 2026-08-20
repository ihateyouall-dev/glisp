#include "builtins.h"
#include "diagnostics.h"
#include "location.h"
#include "memory.h"
#include "runtime/value.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

static const char *__gl_value_get_type_name(gl_value_t *val) {
    switch (val->type) {
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
    case GL_VAL_NIL:
        return "nil";
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

GL_BUILTIN(add) {
    gl_value_t *res = gl_value_make_int(0);

    if (args->size == 0) {
        return res;
    }

    int64_t isum = 0;
    long double fsum = 0;
    for (size_t i = 0; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (res->type == GL_VAL_INT) {
            if (current->val->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum += gl_value_get_int(current->val);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->val->type == GL_VAL_INT) {
                fsum += (long double)gl_value_get_int(current->val);
            } else {
                fsum += gl_value_get_float(current->val);
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        gl_value_set_int(res, isum);
    } else {
        res->val = malloc(sizeof(long double));
        gl_value_set_float(res, fsum);
    }
    return res;
}

GL_BUILTIN(sub) {
    gl_value_t *res = gl_value_make_int(0);

    if (args->size == 0) {
        return res;
    }

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
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    // Unary operator handling
    if (args->size == 1) {
        if (res->type == GL_VAL_INT) {
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
        if (res->type == GL_VAL_INT) {
            if (current->val->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum -= gl_value_get_int(current->val);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->val->type == GL_VAL_INT) {
                fsum -= (long double)gl_value_get_int(current->val);
            } else {
                fsum -= gl_value_get_float(current->val);
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        gl_value_set_int(res, isum);
    } else {
        res->val = malloc(sizeof(long double));
        gl_value_set_float(res, fsum);
    }
    return res;
}

GL_BUILTIN(mul) {
    gl_value_t *res = gl_value_make_int(0);

    if (args->size == 0) {
        return res;
    }

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
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    for (size_t i = 1; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (res->type == GL_VAL_INT) {
            if (current->val->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum *= gl_value_get_int(current->val);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->val->type == GL_VAL_INT) {
                fsum *= (long double)gl_value_get_int(current->val);
            } else {
                fsum *= gl_value_get_float(current->val);
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        gl_value_set_int(res, isum);
    } else {
        res->val = malloc(sizeof(long double));
        gl_value_set_float(res, fsum);
    }
    return res;
}

GL_BUILTIN(div) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;

    EXPECT_VARGS(1);

    if (args->data[0]->val->type != GL_VAL_INT && args->data[0]->val->type != GL_VAL_FLOAT) {
        __emit_type_error(args->data[0]->location, "number",
                          __gl_value_get_type_name(args->data[0]->val));
        return NULL;
    }
    if (args->data[0]->val->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]->val);
    } else {
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]->val);
    }
    for (size_t i = 1; i < args->size; ++i) {
        gl_function_arg_t *current = *(args->data + i);
        if (current->val->type != GL_VAL_INT && current->val->type != GL_VAL_FLOAT) {
            __emit_type_error(current->location, "number", __gl_value_get_type_name(current->val));
            return NULL;
        }
        if (res->type == GL_VAL_INT) {
            if (current->val->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum /= gl_value_get_int(current->val);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->val->type == GL_VAL_INT) {
                fsum /= (long double)gl_value_get_int(current->val);
            } else {
                fsum /= gl_value_get_float(current->val);
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        gl_value_set_int(res, isum);
    } else {
        res->val = malloc(sizeof(long double));
        gl_value_set_float(res, fsum);
    }
    return res;
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

    return gl_value_make_symbol(__gl_value_get_type_name(args->data[0]->val));
}

GL_BUILTIN(int_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_INT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(float_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_FLOAT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(number_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_INT && args->data[i]->val->type != GL_VAL_FLOAT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(symbol_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_SYMBOL) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(string_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_STRING) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(nil_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_NIL) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(function_p) {
    EXPECT_VARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_FUNCTION &&
            args->data[i]->val->type != GL_VAL_BUILTIN &&
            args->data[i]->val->type != GL_VAL_SPFORM) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(list_p) {
    EXPECT_ARGS(1);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->val->type != GL_VAL_CONS) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
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
            current = (gl_value_cons_t *)current->cdr->val;
        }
    }
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = list;
    return res;
}

GL_BUILTIN(gc_collect) {
    EXPECT_ARGS(0);

    gl_gc_collect();

    return gl_value_make_nil();
}

GL_BUILTIN(gc_allocated) {
    EXPECT_ARGS(0);

    return gl_value_make_int(gl_memory_heap->allocated);
}
