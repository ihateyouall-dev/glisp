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

GL_BUILTIN(mul) {
    assert(args->data);
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    if (args->data[0]->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]);
    } else {
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]);
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
                isum *= gl_value_get_int(current);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum *= (long double)gl_value_get_int(current);
            } else {
                fsum *= gl_value_get_float(current);
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
    assert(args->data);
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    if (args->data[0]->type == GL_VAL_INT) {
        isum = gl_value_get_int(args->data[0]);
    } else {
        res->type = GL_VAL_FLOAT;
        fsum = gl_value_get_float(args->data[0]);
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
                isum /= gl_value_get_int(current);
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum /= (long double)gl_value_get_int(current);
            } else {
                fsum /= gl_value_get_float(current);
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
    assert(args->data);
    assert(args->size == 2);

    gl_value_t *lhs = args->data[0];
    gl_value_t *rhs = args->data[1];
    assert(lhs->type == GL_VAL_INT);
    assert(rhs->type == GL_VAL_INT);

    return gl_value_make_int(gl_value_get_int(lhs) % gl_value_get_int(rhs));
}

GL_BUILTIN(eq) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (gl_value_compare(first, second) != 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(gt) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (gl_value_compare(first, second) <= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(lt) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (gl_value_compare(first, second) >= 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(ge) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (gl_value_compare(first, second) < 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(le) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (gl_value_compare(first, second) > 0) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(not ) {
    assert(args->data);

    int val = gl_value_get_bool(args->data[0]);

    return gl_value_make_bool(!val);
}

GL_BUILTIN(and) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (!(gl_value_get_bool(first) && gl_value_get_bool(second))) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(or) {
    assert(args->data);

    gl_value_t *first = args->data[0];
    for (size_t i = 1; i < args->size; ++i) {
        gl_value_t *second = args->data[i];

        if (!(gl_value_get_bool(first) || gl_value_get_bool(second))) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(typeof) {
    assert(args->data);
    assert(args->size == 1);

    switch (args->data[0]->type) {
    case GL_VAL_INT:
        return gl_value_make_symbol("int");
    case GL_VAL_FLOAT:
        return gl_value_make_symbol("float");
    case GL_VAL_SYMBOL:
        return gl_value_make_symbol("symbol");
    case GL_VAL_NIL:
        return gl_value_make_symbol("nil");
    case GL_VAL_FUNCTION:
        return gl_value_make_symbol("function");
    case GL_VAL_BUILTIN:
        return gl_value_make_symbol("builtin");
    case GL_VAL_SPFORM:
        return gl_value_make_symbol("specform");
    default:
        return gl_value_make_nil();
    }
}

GL_BUILTIN(int_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_INT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(float_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_FLOAT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(number_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_INT && args->data[i]->type != GL_VAL_FLOAT) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(symbol_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_SYMBOL) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(nil_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_NIL) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(function_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_FUNCTION && args->data[i]->type != GL_VAL_BUILTIN &&
            args->data[i]->type != GL_VAL_SPFORM) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}

GL_BUILTIN(list_p) {
    assert(args->data);

    for (size_t i = 0; i < args->size; ++i) {
        if (args->data[i]->type != GL_VAL_CONS) {
            return gl_value_make_bool(0);
        }
    }
    return gl_value_make_bool(1);
}
