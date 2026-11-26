/**
 * @file env.c
 * @brief Lexical environments and the initial global environment.
 */

#include "env.h"

#include "runtime/builtins.h"
#include "runtime/specforms.h"
#include "runtime/value.h"
#include "utils/alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HASHMAP_DEFINE(gl_value_t *, gl_value_table) // NOLINT(clang-analyzer-core.NullDereference)

gl_value_t *gl_global_env = NULL;

// Each frame has separate variable and function tables. The function table and
// the constants table are created on first use: a function frame binds
// parameters far more often than it defines functions, and an empty table per
// frame is not free.
//
// The variable table lives in the same allocation as the frame. A call creates
// a frame on every invocation, so one malloc instead of two matters here.
typedef struct {
    gl_env_t env;
    gl_value_table_t variables;
} gl_env_frame_t;

gl_env_t *gl_make_env(gl_value_t *parent) {
    gl_env_frame_t *frame = gl_xmalloc(sizeof(gl_env_frame_t));
    gl_env_t *res = &frame->env;

    res->parent = parent;
    gl_value_table_init(&frame->variables, NULL, gl_value_copy);
    res->variables = &frame->variables;

    res->functions = NULL;
    res->constants = NULL;

    res->params = NULL;
    res->param_count = 0;
    res->param_capacity = 0;

    return res;
}

static gl_value_table_t *__gl_env_functions(gl_env_t *env) {
    if (env->functions == NULL) {
        env->functions = gl_xmalloc(sizeof(gl_value_table_t));
        gl_value_table_init(env->functions, NULL, gl_value_copy);
    }
    return env->functions;
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
    DEFINE_BUILTIN("copy", copy);

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
    DEFINE_BUILTIN("sort", sort);

    DEFINE_BUILTIN("hash", hash);
    DEFINE_BUILTIN("hash-set", hash_set);
    DEFINE_BUILTIN("hash-get", hash_get);
    DEFINE_BUILTIN("hash-has?", hash_has_p);
    DEFINE_BUILTIN("hash-delete", hash_delete);
    DEFINE_BUILTIN("hash-keys", hash_keys);

    DEFINE_BUILTIN("string-append", string_append);
    DEFINE_BUILTIN("substring", substring);
    DEFINE_BUILTIN("string-upcase", string_upcase);
    DEFINE_BUILTIN("string-downcase", string_downcase);
    DEFINE_BUILTIN("string->number", string_to_number);

    DEFINE_BUILTIN("apply", apply);
    DEFINE_BUILTIN("load", load);
    DEFINE_BUILTIN("read", read);
    DEFINE_BUILTIN("eval", eval);

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
    DEFINE_BUILTIN("macroexpand-1", macroexpand_1);
    DEFINE_BUILTIN("macroexpand", macroexpand);

    DEFINE_BUILTIN("gensym", gensym);
}

static void __gl_global_env_define_specforms(gl_env_t *env) {
    DEFINE_SPECFORM("if", if);
    DEFINE_SPECFORM("defun", defun);
    DEFINE_SPECFORM("defmacro", defmacro);
    DEFINE_SPECFORM("lambda", lambda);
    DEFINE_SPECFORM("set", set);
    DEFINE_SPECFORM("set-global", set_global);
    DEFINE_SPECFORM("progn", progn);
    DEFINE_SPECFORM("while", while);
    DEFINE_SPECFORM("quote", quote);
    DEFINE_SPECFORM("quasiquote", quasiquote);
    DEFINE_SPECFORM("unquote", unquote);
    DEFINE_SPECFORM("unquote-splicing", unquote_splicing);
    DEFINE_SPECFORM("function", function);
    DEFINE_SPECFORM("let", let);
    DEFINE_SPECFORM("let*", let_star);
    DEFINE_SPECFORM("pragma", pragma);
    DEFINE_SPECFORM("for", for);
    DEFINE_SPECFORM("add-to-list", add_to_list);
    DEFINE_SPECFORM("defconst", defconst);
    DEFINE_SPECFORM("defconstant", defconst);
    DEFINE_SPECFORM("defvar", defvar);
}

// The global frame starts with singleton constants, typeids, standard streams,
// then native builtins and special forms.
gl_env_t *gl_make_global_env(void) {
    gl_env_t *res = gl_make_env(NULL);

    gl_env_set_const(res, "t", gl_value_make_bool(1));
    gl_env_set_const(res, "else", gl_value_make_bool(1));
    gl_env_set_const(res, "nil", gl_value_make_nil());

    // Basic type identifiers
    DEFINE_TYPEID("nil-t", GL_VAL_NIL);
    DEFINE_TYPEID("int-t", GL_VAL_INT);
    DEFINE_TYPEID("bignum-t", GL_VAL_BIGNUM);
    DEFINE_TYPEID("float-t", GL_VAL_FLOAT);
    DEFINE_TYPEID("symbol-t", GL_VAL_SYMBOL);
    DEFINE_TYPEID("string-t", GL_VAL_STRING);
    DEFINE_TYPEID("function-t", GL_VAL_FUNCTION);
    DEFINE_TYPEID("list-t", GL_VAL_CONS);
    DEFINE_TYPEID("array-t", GL_VAL_ARRAY);
    DEFINE_TYPEID("vector-t", GL_VAL_VECTOR);
    DEFINE_TYPEID("hashmap-t", GL_VAL_HASHMAP);
    DEFINE_TYPEID("file-t", GL_VAL_FILE);
    DEFINE_TYPEID("typeid-t", GL_VAL_TYPEID);
    DEFINE_TYPEID("macro-t", GL_VAL_MACRO);

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
    // The variable table is embedded in the frame allocation.
    gl_value_table_destroy(env->variables);
    if (env->functions) {
        gl_value_table_destroy(env->functions);
        free(env->functions);
        env->functions = NULL;
    }
    if (env->constants) {
        gl_value_table_destroy(env->constants);
        free(env->constants);
        env->constants = NULL;
    }
    free(env->params);
    env->params = NULL;
    env->param_capacity = 0;
    env->param_count = 0;
    free(env);
    env = NULL;
}

// Returns the slot that holds @p sym, or NULL when the frame does not bind it.
// Callers write through the returned pointer, which also tells the static
// analyser that the slot is inside a live array.
static gl_value_t **__gl_env_param_slot(gl_env_t *env, const char *sym) {
    if (env == NULL || env->params == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < env->param_count; ++i) {
        if (strcmp(env->params[i].name, sym) == 0) {
            return &env->params[i].value;
        }
    }
    return NULL;
}

void gl_env_bind_param(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_value_t **slot = __gl_env_param_slot(env, sym);
    if (slot != NULL) {
        *slot = val;
        return;
    }

    if (env->param_count == env->param_capacity) {
        size_t capacity = env->param_capacity == 0 ? 4 : env->param_capacity * 2;
        env->params = gl_xrealloc(env->params, capacity * sizeof(gl_env_param_t));
        env->param_capacity = capacity;
    }
    env->params[env->param_count].name = sym;
    env->params[env->param_count].value = val;
    ++env->param_count;
}

void gl_env_set_var(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_value_t **slot = __gl_env_param_slot(env, sym);
    if (slot != NULL) {
        *slot = val;
        return;
    }
    gl_value_table_insert(env->variables, sym, val);
}

int gl_env_has_local_var(gl_env_t *env, const char *sym) {
    if (env == NULL) {
        return 0;
    }
    if (__gl_env_param_slot(env, sym) != NULL) {
        return 1;
    }
    return gl_value_table_get(env->variables, sym) != NULL;
}

void gl_env_set_fun(gl_env_t *env, const char *sym, gl_value_t *val) {
    assert(val->type == GL_VAL_BUILTIN || val->type == GL_VAL_FUNCTION ||
           val->type == GL_VAL_SPFORM || val->type == GL_VAL_MACRO);
    gl_value_table_insert(__gl_env_functions(env), sym, val);
}

// Constants are stored both as variables and in the constants table.
void gl_env_set_const(gl_env_t *env, const char *sym, gl_value_t *val) {
    gl_env_set_var(env, sym, val);
    if (env->constants == NULL) {
        gl_value_table_t *constants = gl_xmalloc(sizeof(gl_value_table_t));
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

// Lookups walk the parent chain; a NULL result means the name is unbound.
gl_value_t *gl_env_get_var(gl_env_t *env, const char *sym) {
    while (env != NULL) {
        gl_value_t **slot = __gl_env_param_slot(env, sym);
        if (slot != NULL && *slot != NULL) {
            return *slot;
        }

        gl_value_t **obj = gl_value_table_get(env->variables, sym);
        if (obj != NULL && *obj != NULL) {
            return *obj;
        }

        env = env->parent != NULL ? (gl_env_t *)env->parent->val : NULL;
    }
    return NULL;
}

gl_value_t *gl_env_get_callable(gl_env_t *env, const char *sym) {
    while (env != NULL) {
        gl_value_t **fun = env->functions ? gl_value_table_get(env->functions, sym) : NULL;
        if (fun != NULL && *fun != NULL) {
            return *fun;
        }

        gl_value_t **slot = __gl_env_param_slot(env, sym);
        if (slot != NULL && *slot != NULL) {
            return *slot;
        }

        gl_value_t **var = gl_value_table_get(env->variables, sym);
        if (var != NULL && *var != NULL) {
            return *var;
        }

        env = env->parent ? (gl_env_t *)env->parent->val : NULL;
    }
    return NULL;
}

gl_value_t *gl_env_get_fun(gl_env_t *env, const char *sym) {
    gl_value_t **obj = env->functions ? gl_value_table_get(env->functions, sym) : NULL;

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

// Levenshtein distance, abandoned as soon as it can no longer stay under the
// limit. Names in this language are short, so a small cap is enough.
static size_t __gl_edit_distance(const char *a, const char *b, size_t limit) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (len_a > len_b + limit || len_b > len_a + limit) {
        return limit + 1;
    }

    size_t prev[64];
    size_t curr[64];
    if (len_b + 1 > sizeof(prev) / sizeof(prev[0])) {
        return limit + 1;
    }

    for (size_t j = 0; j <= len_b; ++j) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= len_a; ++i) {
        curr[0] = i;
        size_t best = curr[0];
        for (size_t j = 1; j <= len_b; ++j) {
            size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            size_t deletion = prev[j] + 1;
            size_t insertion = curr[j - 1] + 1;
            size_t substitution = prev[j - 1] + cost;

            size_t value = deletion < insertion ? deletion : insertion;
            if (substitution < value) {
                value = substitution;
            }
            curr[j] = value;
            if (value < best) {
                best = value;
            }
        }
        if (best > limit) {
            return limit + 1;
        }
        for (size_t j = 0; j <= len_b; ++j) {
            prev[j] = curr[j];
        }
    }

    return prev[len_b];
}

static void __gl_consider_name(const char *candidate, const char *sym, size_t *best_distance,
                               char **best) {
    // Short names get a tighter limit, otherwise a three-letter typo matches
    // half the standard library.
    size_t limit = strlen(sym) <= 3 ? 1 : 2;
    size_t distance = __gl_edit_distance(sym, candidate, limit);

    if (distance > limit) {
        return;
    }
    if (*best == NULL || distance < *best_distance) {
        free(*best);
        *best = strdup(candidate);
        *best_distance = distance;
    }
}

static void __gl_scan_table(gl_value_table_t *table, const char *sym, size_t *best_distance,
                            char **best) {
    if (table == NULL || table->buckets == NULL) {
        return;
    }

    for (size_t i = 0; i < table->buckets_count; ++i) {
        gl_value_tableBucket_t *bucket = &table->buckets[i];
        for (size_t j = 0; j < bucket->size; ++j) {
            gl_value_tableEntry_t *entry = gl_value_tableBucket_at(bucket, j);
            if (entry != NULL) {
                __gl_consider_name(entry->key, sym, best_distance, best);
            }
        }
    }
}

char *gl_env_suggest_name(gl_env_t *env, const char *sym) {
    size_t best_distance = 3;
    char *best = NULL;

    while (env != NULL) {
        __gl_scan_table(env->functions, sym, &best_distance, &best);
        __gl_scan_table(env->variables, sym, &best_distance, &best);
        for (size_t i = 0; i < env->param_count; ++i) {
            __gl_consider_name(env->params[i].name, sym, &best_distance, &best);
        }

        env = env->parent ? (gl_env_t *)env->parent->val : NULL;
    }

    return best;
}
