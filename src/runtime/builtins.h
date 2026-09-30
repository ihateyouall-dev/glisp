#pragma once

#include "location.h"
#include "value.h"

#define GL_BUILTIN(Name)                                                                           \
    gl_value_t *gl_builtin_##Name(gl_location_t called_at, gl_function_args_t *args)

GL_BUILTIN(exit);

GL_BUILTIN(print);

GL_BUILTIN(println);

GL_BUILTIN(car);

GL_BUILTIN(cdr);

GL_BUILTIN(cons);

// Arithmetic operator +
GL_BUILTIN(add);

// Arithmetic operator -
GL_BUILTIN(sub);

// Arithmetic operator *
GL_BUILTIN(mul);

// Arithmetic operator /
GL_BUILTIN(div);

// Arithmetic operator % (mod)
GL_BUILTIN(mod);

// Logical equation
GL_BUILTIN(eq);

// Logical >
GL_BUILTIN(gt);

// Logical <
GL_BUILTIN(lt);

// Logical >=
GL_BUILTIN(ge);

// Logical <=
GL_BUILTIN(le);

// Return symbol representing type of arg
GL_BUILTIN(typeof);

// Type constructors

GL_BUILTIN(int_con);

GL_BUILTIN(float_con);

GL_BUILTIN(list_con);

GL_BUILTIN(array_con);

GL_BUILTIN(vector_con);

// Array and vector operations

GL_BUILTIN(len);

GL_BUILTIN(get);

GL_BUILTIN(set_at);

// Dynamic vector operations

GL_BUILTIN(vector_push);

GL_BUILTIN(vector_pop);

GL_BUILTIN(apply);

GL_BUILTIN(load);

// Loads and executes a Lisp file.
// If caller_unit is provided, relative paths resolve relative to caller_unit's directory.
// Returns evaluated value or NULL on error.
gl_value_t *gl_load_file(const char *filepath, const char *caller_unit);

// Autoloads standard library. Checks GLISP_STD_PATH, GLISP_STD, then defaults.
// Returns 0 on success, -1 on failure.
int gl_load_stdlib(void);

GL_BUILTIN(assert);
