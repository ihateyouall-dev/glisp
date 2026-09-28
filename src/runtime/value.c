#include "value.h"

#include "ast.h"
#include "env.h"
#include "memory.h"

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
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_float(long double num) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_FLOAT;
    res->val = malloc(sizeof(long double));
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
    case GL_AST_STRING:
        car = gl_value_make_string(cons.car->value.string);
        break;
    case GL_AST_CONS:
        car = gl_value_make_cons(cons.car->value.cons);
        break;
    case GL_AST_ARRAY:
        break;
    case GL_AST_VECTOR:
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

    gl_register_value(res);
    return res;
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

gl_value_t *gl_value_make_string(char *str) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_STRING;
    res->val = strdup(str);
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_nil(void) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    res->type = GL_VAL_NIL;
    res->val = NULL;
    gl_register_value(res);
    return res;
}

gl_value_t *gl_value_make_bool(int b) {
    if (b) {
        return gl_value_make_symbol("t");
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

void gl_function_arg_free(gl_function_arg_t **arg) {
    if (arg && *arg) {
        free(*arg);
        *arg = NULL;
    }
}

gl_value_t *gl_value_copy(gl_value_t *val) {
    gl_value_t *res = malloc(sizeof(gl_value_t));
    assert(val);
    assert(val->type == GL_VAL_NIL || val->val);
    res->type = val->type;
    res->gc_marked = val->gc_marked;

    switch (val->type) {
    case GL_VAL_INT:
        res->val = malloc(sizeof(int64_t));
        *(int64_t *)res->val = *(int64_t *)val->val;
        break;
    case GL_VAL_FLOAT:
        res->val = malloc(sizeof(long double));
        *(long double *)res->val = *(long double *)val->val;
        break;
    case GL_VAL_TYPEID:
        res->val = malloc(sizeof(gl_value_type_t));
        *(gl_value_type_t *)res->val = *(gl_value_type_t *)val->val;
        break;
    case GL_VAL_CONS:
        res->val = malloc(sizeof(gl_value_cons_t));
        ((gl_value_cons_t *)res->val)->car = gl_value_copy(((gl_value_cons_t *)val->val)->car);
        ((gl_value_cons_t *)res->val)->cdr = gl_value_copy(((gl_value_cons_t *)val->val)->cdr);
        break;
    case GL_VAL_STRING:
    case GL_VAL_SYMBOL:
        res->val = strdup((char *)val->val);
        break;
    case GL_VAL_FUNCTION:
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
    }
    gl_register_value(res);
    return res;
}

void gl_value_destroy(gl_value_t **val) {
    if (val == NULL || *val == NULL)
        return;

    switch ((*val)->type) {
    case GL_VAL_INT:
    case GL_VAL_FLOAT:
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

static const char *__gl_typeid_str(gl_value_type_t type) {
    switch (type) {
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

void gl_value_print(gl_value_t *val) {
    if (!val) {
        return;
    }
    switch (val->type) {
    case GL_VAL_INT:
        printf("%lld", (*(int64_t *)val->val));
        break;
    case GL_VAL_FLOAT:
        printf("%Lf", (*(long double *)val->val));
        break;
    case GL_VAL_TYPEID:
        printf("%s", __gl_typeid_str(*(gl_value_type_t *)val->val));
        break;
    case GL_VAL_STRING:
    case GL_VAL_SYMBOL:
        printf("%s", (char *)val->val);
        break;
    case GL_VAL_NIL:
        printf("nil");
        break;
    case GL_VAL_BUILTIN:
    case GL_VAL_FUNCTION:
    case GL_VAL_SPFORM:
    case GL_VAL_ENV:
        // Nothing to print now
        break;
    case GL_VAL_CONS: {
        printf("(");
        gl_value_t *cur = val;
        int first = 1;
        while (cur && cur->type == GL_VAL_CONS) {
            if (!first) {
                printf(" ");
            }
            first = 0;
            gl_value_cons_t *cons = cur->val;
            gl_value_print(cons->car);
            cur = cons->cdr;
        }
        if (cur && cur->type != GL_VAL_NIL) {
            printf(" . ");
            gl_value_print(cur);
        }
        printf(")");
        break;
    }
    case GL_VAL_ARRAY: {
        printf("[");
        gl_value_array_t *arr = val->val;
        if (arr && arr->size > 0) {
            for (size_t i = 0; i < arr->size; ++i) {
                gl_value_print(arr->data[i]);
                if (i + 1 < arr->size) {
                    printf(" ");
                }
            }
        }
        printf("]");
        break;
    }
    case GL_VAL_VECTOR: {
        printf("{");
        gl_value_vector_t *vec = val->val;
        if (vec && vec->size > 0) {
            for (size_t i = 0; i < vec->size; ++i) {
                gl_value_print(*gl_value_vector_at(vec, i));
                if (i + 1 < vec->size) {
                    printf(" ");
                }
            }
        }
        printf("}");
        break;
    }
    }
}
int64_t gl_value_get_int(gl_value_t *val) {
    assert(val->type == GL_VAL_INT);
    return *(int64_t *)val->val;
}
long double gl_value_get_float(gl_value_t *val) {
    assert(val->type == GL_VAL_FLOAT);
    return *(long double *)val->val;
}
char *gl_value_get_symbol(gl_value_t *val) {
    assert(val->type == GL_VAL_SYMBOL);
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
gl_value_type_t gl_value_get_typeid(gl_value_t *val) {
    assert(val->type == GL_VAL_TYPEID);
    return *(gl_value_type_t *)val->val;
}

gl_builtin_t gl_value_get_builtin(gl_value_t *val) {
    assert(val->type == GL_VAL_BUILTIN);
    return val->val;
}
gl_function_t *gl_value_get_function(gl_value_t *val) {
    assert(val->type == GL_VAL_FUNCTION);
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
    return val->type == GL_VAL_INT || val->type == GL_VAL_FLOAT;
}

static int __gl_value_compare_numbers(gl_value_t *lhs, gl_value_t *rhs) {
    if (lhs->type == GL_VAL_INT) {
        int lhs_val = gl_value_get_int(lhs);
        if (rhs->type == GL_VAL_INT) { // Comparing INT to INT
            int rhs_val = gl_value_get_int(rhs);

            if (lhs_val == rhs_val) {
                return 0;
            } else if (lhs_val > rhs_val) {
                return 1;
            } else {
                return -1;
            }
        } else { // Comparing INT to FLOAT
            long double rhs_val = gl_value_get_float(rhs);

            if ((long double)lhs_val == rhs_val) {
                return 0;
            } else if ((long double)lhs_val > rhs_val) {
                return 1;
            } else {
                return -1;
            }
        }
    } else {
        long double lhs_val = gl_value_get_float(lhs);
        if (rhs->type == GL_VAL_INT) { // Comparing FLOAT to INT
            int rhs_val = gl_value_get_int(rhs);

            if (lhs_val == (long double)rhs_val) {
                return 0;
            } else if (lhs_val > (long double)rhs_val) {
                return 1;
            } else {
                return -1;
            }
        } else { // Comparing FLOAT to FLOAT
            long double rhs_val = gl_value_get_float(rhs);

            if (lhs_val == rhs_val) {
                return 0;
            } else if (lhs_val > rhs_val) {
                return 1;
            } else {
                return -1;
            }
        }
    }
}

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

    if (lhs->type == GL_VAL_BUILTIN || lhs->type == GL_VAL_SPFORM || lhs->type == GL_VAL_FUNCTION) {
        if (lhs->type == rhs->type) {
            if (lhs->val == rhs->val) {
                return 0;
            }
        }
        return -1;
    }
    return -1;
}
