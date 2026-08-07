#include "builtins.h"
#include "runtime/value.h"
#include <stdint.h>
#include <stdio.h>

ARRAY_DEFINE(gl_value_t *, gl_value_array)

GL_BUILTIN(exit) {
    assert(args);
    assert(args->data[0]->val);
    exit(gl_value_get_int(args->data[0]));
}

GL_BUILTIN(print) {
    assert(args->data);
    assert(args->size > 0);

    for (size_t i = 0; i < args->size; ++i) {
        gl_value_print(args->data[i]);
    }
    return gl_value_make_nil();
}

GL_BUILTIN(println) {
    if (!args->data || args->size == 0) {
        printf("\n");
        return gl_value_make_nil();
    }
    gl_builtin_print(args);
    printf("\n");
    return gl_value_make_nil();
}

GL_BUILTIN(car) {
    assert(args);
    assert(args->size == 1);
    assert(args->data[0]->type == GL_VAL_CONS);

    gl_value_cons_t *cons = gl_value_get_cons(args->data[0]);

    return cons->car;
}

GL_BUILTIN(cdr) {
    assert(args);
    assert(args->size == 1);
    assert(args->data[0]->type == GL_VAL_CONS);

    gl_value_cons_t *cons = gl_value_get_cons(args->data[0]);

    return cons->cdr;
}

GL_BUILTIN(add) {
    assert(args->data);
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    for (size_t i = 0; i < args->size; ++i) {
        gl_value_t *current = *(args->data + i);
        assert(current->type == GL_VAL_INT || current->type == GL_VAL_FLOAT);
        if (res->type == GL_VAL_INT) {
            if (current->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum += gl_value_get_int(current);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum += (long double)gl_value_get_int(current);
            } else {
                fsum += gl_value_get_float(current);
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
    assert(args);
    assert(args->data);
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    // Substraction goes from first arg
    if (args->data[0]->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]);
    } else {
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]);
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
        gl_value_t *current = *(args->data + i);
        assert(current->type == GL_VAL_INT || current->type == GL_VAL_FLOAT);
        if (res->type == GL_VAL_INT) {
            if (current->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum -= gl_value_get_int(current);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum -= (long double)gl_value_get_int(current);
            } else {
                fsum -= gl_value_get_float(current);
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
