#include "env.h"

#include "runtime/builtins.h"
#include "runtime/specforms.h"
#include "runtime/value.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t *, gl_value_table)

gl_value_t *gl_global_env = NULL;

gl_env_t *gl_make_env(gl_value_t *parent) {
    gl_env_t *res = malloc(sizeof(gl_env_t));
    res->parent = parent;

    gl_value_table_t *values = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(values, NULL, gl_value_copy);
    res->variables = values;

    gl_value_table_t *functions = malloc(sizeof(gl_value_table_t));
    gl_value_table_init(functions, NULL, gl_value_copy);
    res->functions = functions;

    res->constants = NULL;

    return res;
}

#define DEFINE_BUILTIN(Name, CName)                                                                \
    do {                                                                                           \
        gl_env_set_fun(env, Name, gl_value_make_builtin(gl_builtin_##CName));                      \
    } while (0)

#define DEFINE_SPECFORM(Name, CName)                                                               \
    do {                                                                                           \
        gl_env_set_fun(env, Name, gl_value_make_specform(&gl_specform_##CName));                   \
    } while (0)

#define DEFINE_TYPEID(Name, CName)                                                                 \
    do {                                                                                           \
        gl_env_set_const(res, Name, gl_value_make_typeid(CName));                                  \
    } while (0)

static void __gl_global_env_define_builtins(gl_env_t *env) {
    DEFINE_BUILTIN("exit", exit);
    DEFINE_BUILTIN("typeof", typeof);

    DEFINE_BUILTIN("+", add);
    DEFINE_BUILTIN("-", sub);
    DEFINE_BUILTIN("*", mul);
    DEFINE_BUILTIN("/", div);
    DEFINE_BUILTIN("%", mod);

    DEFINE_BUILTIN("=", eq);
    DEFINE_BUILTIN("eq", eq);
    DEFINE_BUILTIN(">", gt);
    DEFINE_BUILTIN("<", lt);
    DEFINE_BUILTIN(">=", ge);
    DEFINE_BUILTIN("<=", le);

    DEFINE_BUILTIN("car", car);
    DEFINE_BUILTIN("cdr", cdr);
    DEFINE_BUILTIN("cons", cons);

    DEFINE_BUILTIN("int", int_con);
    DEFINE_BUILTIN("float", float_con);
    DEFINE_BUILTIN("array", array_con);
    DEFINE_BUILTIN("vector", vector_con);

    DEFINE_BUILTIN("len", len);
    DEFINE_BUILTIN("length", len);
    DEFINE_BUILTIN("get", get);
    DEFINE_BUILTIN("nth", get);
    DEFINE_BUILTIN("aref", get);
    DEFINE_BUILTIN("set-at", set_at);
    DEFINE_BUILTIN("aset", set_at);
    DEFINE_BUILTIN("push", vector_push);
    DEFINE_BUILTIN("vector-push", vector_push);
    DEFINE_BUILTIN("pop", vector_pop);
    DEFINE_BUILTIN("vector-pop", vector_pop);

    DEFINE_BUILTIN("apply", apply);
    DEFINE_BUILTIN("load", load);

    DEFINE_BUILTIN("assert", assert);

    DEFINE_BUILTIN("sqrt", sqrt);
    DEFINE_BUILTIN("fma", fma);
    DEFINE_BUILTIN("copysign", copysign);
    DEFINE_BUILTIN("exp", exp);
    DEFINE_BUILTIN("exp2", exp2);
    DEFINE_BUILTIN("log", log);
    DEFINE_BUILTIN("log2", log2);
    DEFINE_BUILTIN("floor", floor);
    DEFINE_BUILTIN("ceil", ceil);
    DEFINE_BUILTIN("round", round);
    DEFINE_BUILTIN("sin", sin);
    DEFINE_BUILTIN("cos", cos);
    DEFINE_BUILTIN("atan2", atan2);
    DEFINE_BUILTIN("pow", pow);

    DEFINE_BUILTIN("open", open);
    DEFINE_BUILTIN("close", close);
    DEFINE_BUILTIN("write-string", write_str);
    DEFINE_BUILTIN("to-string", to_str);
    DEFINE_BUILTIN("read-line", read_line);
    DEFINE_BUILTIN("open?", open_p);
    DEFINE_BUILTIN("file-mode", file_mode);
    DEFINE_BUILTIN("eof?", eof_p);
    DEFINE_BUILTIN("flush", flush);

    DEFINE_BUILTIN("logand", logand);
    DEFINE_BUILTIN("logior", logior);
    DEFINE_BUILTIN("logxor", logxor);
    DEFINE_BUILTIN("lognot", lognot);
}

static void __gl_global_env_define_specforms(gl_env_t *env) {
    DEFINE_SPECFORM("if", if);
    DEFINE_SPECFORM("defun", defun);
    DEFINE_SPECFORM("lambda", lambda);
    DEFINE_SPECFORM("set", set);
    DEFINE_SPECFORM("set-global", set_global);
    DEFINE_SPECFORM("progn", progn);
    DEFINE_SPECFORM("while", while);
    DEFINE_SPECFORM("quote", quote);
    DEFINE_SPECFORM("function", function);
    DEFINE_SPECFORM("let", let);
    DEFINE_SPECFORM("let*", let_star);
    DEFINE_SPECFORM("pragma", pragma);
    DEFINE_SPECFORM("cond", cond);
    DEFINE_SPECFORM("for", for);
    DEFINE_SPECFORM("and", and);
    DEFINE_SPECFORM("or", or);
    DEFINE_SPECFORM("add-to-list", add_to_list);
    DEFINE_SPECFORM("defconst", defconst);
    DEFINE_SPECFORM("defconstant", defconst);
}

gl_env_t *gl_make_global_env(void) {
    gl_env_t *res = gl_make_env(NULL);

    gl_env_set_const(res, "t", gl_value_make_bool(1));
    gl_env_set_const(res, "else", gl_value_make_bool(1));
    gl_env_set_const(res, "nil", gl_value_make_nil());

    // Basic type identifiers
    DEFINE_TYPEID("nil-t", GL_VAL_NIL);
    DEFINE_TYPEID("int-t", GL_VAL_INT);
    DEFINE_TYPEID("float-t", GL_VAL_FLOAT);
    DEFINE_TYPEID("symbol-t", GL_VAL_SYMBOL);
    DEFINE_TYPEID("string-t", GL_VAL_STRING);
    DEFINE_TYPEID("function-t", GL_VAL_FUNCTION);
    DEFINE_TYPEID("list-t", GL_VAL_CONS);
    DEFINE_TYPEID("array-t", GL_VAL_ARRAY);
    DEFINE_TYPEID("vector-t", GL_VAL_VECTOR);
    DEFINE_TYPEID("file-t", GL_VAL_FILE);
    DEFINE_TYPEID("typeid-t", GL_VAL_TYPEID);

    gl_env_set_const(res, "*stdin*", gl_value_make_file(stdin, "<stdin>", 1, GL_FILE_READ));
    gl_env_set_const(res, "*stdout*", gl_value_make_file(stdout, "<stdout>", 1, GL_FILE_WRITE));
    gl_env_set_const(res, "*stderr*", gl_value_make_file(stderr, "<stderr>", 1, GL_FILE_WRITE));

    __gl_global_env_define_builtins(res);
    __gl_global_env_define_specforms(res);

    return res;
}

void gl_init_global_env(void) { gl_global_env = gl_value_make_env(gl_make_global_env()); }

void gl_env_destroy(gl_env_t *env) {
    if (!env)
        return;
    gl_value_table_destroy(env->variables);
    free(env->variables);
    gl_value_table_destroy(env->functions);
    free(env->functions);
    if (env->constants) {
        gl_value_table_destroy(env->constants);
        free(env->constants);
        env->constants = NULL;
    }
    free(env);
    env = NULL;
}

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_value_table_insert(env->variables, sym, val);
}

void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val) {
    assert(val->type == GL_VAL_BUILTIN || val->type == GL_VAL_FUNCTION ||
           val->type == GL_VAL_SPFORM);
    gl_value_table_insert(env->functions, sym, val);
}

void gl_env_set_const(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_env_set_var(env, sym, val);
    if (env->constants == NULL) {
        gl_value_table_t *constants = malloc(sizeof(gl_value_table_t));
        gl_value_table_init(constants, NULL, gl_value_copy);
        env->constants = constants;
    }
    gl_value_table_insert(env->constants, sym, val);
}

int gl_env_is_const(gl_env_t *env, const char *sym) {
    if (!env || !sym) {
        return 0;
    }
    if (env->constants != NULL && gl_value_table_get(env->constants, sym) != NULL) {
        return 1;
    }
    if (env->parent != NULL) {
        return gl_env_is_const((gl_env_t *)env->parent->val, sym);
    }
    return 0;
}

gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym) {
    gl_value_t **obj = gl_value_table_get(env->variables, sym);

    gl_value_t *res = (obj != NULL ? *obj : NULL);

    // Checking for variable in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_var((gl_env_t *)env->parent->val, sym);
    }
    // If have not found in parent environments, return NULL
    if (res == NULL) {
        return NULL;
    }
    return res;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t **obj = gl_value_table_get(env->functions, sym);

    gl_value_t *res = (obj != NULL ? *obj : NULL);

    // Checking for function in parent environment if not found in current
    if (res == NULL && env->parent != NULL) {
        return gl_env_get_fun((gl_env_t *)env->parent->val, sym);
    }
    // If have not found in parent environments, return NULL
    if (res == NULL) {
        return NULL;
    }
    return res;
}
