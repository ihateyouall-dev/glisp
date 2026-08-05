#include "builtins.h"
#include "runtime/value.h"
#include <stdio.h>

ARRAY_DEFINE(gl_value_t *, gl_value_array)

GL_BUILTIN(exit) {
    assert(args);
    assert(args->data[0]->val);
    exit(*((int *)args->data[0]->val));
}

GL_BUILTIN(print) {
    assert(args->data);
    assert(args->size > 0);

    for (size_t i = 0; i < args->size; ++i) {
        gl_value_print(args->data[i]);
    }
    return NULL;
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
                isum += *(int64_t *)current->val;
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum += (long double)*(int64_t *)current->val;
            } else {
                fsum += *(long double *)current->val;
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        *(int64_t *)res->val = isum;
    } else {
        res->val = malloc(sizeof(long double));
        *(long double *)res->val = fsum;
    }
    return res;
}

// TODO
GL_BUILTIN(sub) {
    assert(args->data);
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    int64_t isum = 0;
    long double fsum = 0;
    // If only 1 argument provided, just return its negative value
    if (args->size == 1) {
    }
    for (size_t i = 0; i < args->size; ++i) {
        gl_value_t *current = *(args->data + i);
        assert(current->type == GL_VAL_INT || current->type == GL_VAL_FLOAT);
        if (res->type == GL_VAL_INT) {
            if (current->type == GL_VAL_FLOAT) {
                // Transfering integral sum to the float
                res->type = GL_VAL_FLOAT;
                fsum = (long double)isum;
            } else {
                isum += *(int64_t *)current->val;
            }
        }
        if (res->type == GL_VAL_FLOAT) {
            if (current->type == GL_VAL_INT) {
                fsum += (long double)*(int64_t *)current->val;
            } else {
                fsum += *(long double *)current->val;
            }
        }
    }
    // Finally assigning sum to result
    if (res->type == GL_VAL_INT) {
        res->val = malloc(sizeof(int64_t));
        *(int64_t *)res->val = isum;
    } else {
        res->val = malloc(sizeof(long double));
        *(long double *)res->val = fsum;
    }
    return res;
}
