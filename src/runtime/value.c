#include "value.h"

#include "ast.h"

#ifdef _WIN32
#include "utils/strdup.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gl_value_t *gl_value_make_int(int64_t num) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_INT;
    res->val = malloc(sizeof(int64_t));
    *(int64_t *)res->val = num;
    return res;
}

gl_value_t *gl_value_make_float(long double num) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FLOAT;
    res->val = malloc(sizeof(long double));
    *(long double *)res->val = num;
    return res;
}

gl_value_t *gl_value_make_symbol(const char *sym) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_SYMBOL;
    res->val = strdup(sym);
    return res;
}

// Create literal cons of values
gl_value_t *gl_value_make_cons(gl_ast_cons_t cons) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_CONS;
    res->val = malloc(sizeof(gl_value_cons_t));
    // Constructing car
    gl_value_t *car;
    switch (cons.car->type) {
    case GL_AST_NIL:
        car = gl_value_make_nil();
        break;
    case GL_AST_INT:
        car = gl_value_make_int(cons.car->value.integral);
        break;
    case GL_AST_FLOAT:
        car = gl_value_make_float(cons.car->value.floating);
        break;
    case GL_AST_SYMBOL:
        car = gl_value_make_symbol(cons.car->value.symbol);
        break;
    case GL_AST_CONS:
        car = gl_value_make_cons(cons.car->value.cons);
        break;
    }
    ((gl_value_cons_t *)res->val)->car = car;

    if (cons.cdr->type == GL_AST_NIL) {
        ((gl_value_cons_t *)res->val)->cdr = gl_value_make_nil();
    } else if (cons.cdr->type == GL_AST_CONS) {
        ((gl_value_cons_t *)res->val)->cdr = gl_value_make_cons(cons.cdr->value.cons);
    } else {
        assert(0 && "CDR of CONS can be only other CONS or NIL");
    }

    return res;
}

gl_value_t *gl_value_make_nil(void) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_NIL;
    res->val = NULL;
    return res;
}

gl_value_t *gl_value_make_builtin(gl_builtin_t builtin) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_BUILTIN;
    res->val = malloc(sizeof(gl_builtin_t));
    *(gl_builtin_t *)res->val = builtin;
    return res;
}

gl_value_t *gl_value_make_function(gl_ast_node_t *tree, gl_function_params_t *params,
                                   gl_env_t *closure) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FUNCTION;
    gl_function_t *func = malloc(sizeof(gl_function_t));
    func->tree = tree;
    func->params = params;
    func->closure = closure;
    res->val = func;
    return res;
}

void gl_function_param_free(char **param) { free(*param); }

gl_value_t *gl_value_copy(gl_value_t *val) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = val->type;
    size_t bytes = 0;

    switch (val->type) {
    case GL_VAL_INT:
        bytes = sizeof(int64_t);
        break;
    case GL_VAL_FLOAT:
        bytes = sizeof(long double);
        break;
    case GL_VAL_CONS:
        res->val = malloc(sizeof(gl_value_cons_t));
        ((gl_value_cons_t *)res->val)->car = gl_value_copy(((gl_value_cons_t *)val->val)->car);
        ((gl_value_cons_t *)res->val)->cdr = gl_value_copy(((gl_value_cons_t *)val->val)->cdr);
        return res;
    case GL_VAL_SYMBOL:
        res->val = strdup((char *)val->val);
        return res;
    case GL_VAL_FUNCTION:
        res->val = malloc(sizeof(gl_function_t));
        gl_function_t *func = res->val;

        func->tree = gl_ast_copy(((gl_function_t *)val->val)->tree);
        func->closure = ((gl_function_t *)val->val)->closure;
        gl_function_params_init(func->params, ((gl_function_t *)val->val)->params->size,
                                &gl_function_param_free);
        for (size_t i = 0; i < func->params->size; ++i) {
            func->params->data[i] = strdup(((gl_function_t *)val->val)->params->data[i]);
        }
        return res;
    case GL_VAL_BUILTIN:
        bytes = sizeof(gl_builtin_t);
        break;
    case GL_VAL_SPFORM:
        bytes = sizeof(gl_special_form_t);
        break;
    case GL_VAL_NIL:
        res->val = NULL;
        return res;
    }
    memcpy(res->val, val->val, bytes);
    return res;
}

void gl_value_destroy(gl_value_t **val) {
    if (val == NULL || *val == NULL)
        return;

    switch ((*val)->type) {
    case GL_VAL_NIL:
        break;
    case GL_VAL_INT:
    case GL_VAL_FLOAT:
    case GL_VAL_SYMBOL:
    case GL_VAL_FUNCTION:
    case GL_VAL_BUILTIN:
        free((*val)->val);
        break;
    case GL_VAL_CONS:
        gl_value_destroy(&((gl_value_cons_t *)((*val)->val))->car);
        gl_value_destroy(&((gl_value_cons_t *)((*val)->val))->cdr);
        free((*val)->val);
    default:
        break;
    }
    free(*val);
    *val = NULL;
}

gl_value_t *gl_value_make_specform(gl_special_form_t spform) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_SPFORM;
    res->val = malloc(sizeof(gl_special_form_t));
    *(gl_special_form_t *)res->val = spform;
    return res;
}

void gl_value_print(gl_value_t *val) {
    switch (val->type) {
    case GL_VAL_INT:
        printf("%lld", (*(int64_t *)val->val));
        break;
    case GL_VAL_FLOAT:
        printf("%Lf", (*(long double *)val->val));
        break;
    case GL_VAL_SYMBOL:
        printf("%s", (char *)val->val);
        break;
    case GL_VAL_NIL:
        printf("nil");
        break;
    case GL_VAL_BUILTIN:
    case GL_VAL_FUNCTION:
    case GL_VAL_SPFORM:
        // Nothing to print now
        break;
    case GL_VAL_CONS:
        printf("(");
        gl_value_cons_t *cons = val->val;

        while (cons->cdr->type != GL_VAL_NIL) {
            gl_value_print(cons->car);

            if (cons->cdr->type != GL_VAL_NIL) {
                printf(" ");
            }
            cons = cons->cdr->val;
        }
        printf(")");
    }
}
