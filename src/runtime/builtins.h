#pragma once

/**
 * @file builtins.h
 * @brief Declarations of native builtins.
 */

#include "location.h"
#include "value.h"

/**
 * Declares a native builtin.
 *
 * Builtins receive already-evaluated arguments and the source location of the
 * call. They return NULL after reporting a diagnostic to signal an error.
 */
#define GL_BUILTIN(Name)                                                                           \
    gl_value_t *gl_builtin_##Name(gl_location_t called_at, gl_function_args_t *args)

GL_BUILTIN(exit);

GL_BUILTIN(car);

GL_BUILTIN(cdr);

GL_BUILTIN(cons);

// Arithmetic

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

// Comparison

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

// Type inspection and construction

// Return symbol representing type of arg
GL_BUILTIN(typeof);

// Type constructors

GL_BUILTIN(int_con);

GL_BUILTIN(float_con);

GL_BUILTIN(list_con);

GL_BUILTIN(array_con);

GL_BUILTIN(vector_con);

// Sequences

// Array and vector operations

GL_BUILTIN(len);

GL_BUILTIN(get);

GL_BUILTIN(set_at);

// Dynamic vector operations

GL_BUILTIN(vector_push);

GL_BUILTIN(vector_pop);

// String operations

GL_BUILTIN(string_append);

GL_BUILTIN(substring);

GL_BUILTIN(string_upcase);

GL_BUILTIN(string_downcase);

// Function application and source loading

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

// Math

GL_BUILTIN(sqrt);

GL_BUILTIN(fma);

GL_BUILTIN(copysign);

GL_BUILTIN(exp);

GL_BUILTIN(exp2);

GL_BUILTIN(log);

GL_BUILTIN(log2);

GL_BUILTIN(floor);

GL_BUILTIN(ceil);

GL_BUILTIN(round);

GL_BUILTIN(sin);

GL_BUILTIN(cos);

GL_BUILTIN(atan2);

GL_BUILTIN(pow);

// I/O

GL_BUILTIN(open);

GL_BUILTIN(close);

GL_BUILTIN(write_str);

GL_BUILTIN(to_str);

GL_BUILTIN(read_line);

GL_BUILTIN(open_p);

GL_BUILTIN(file_mode);

GL_BUILTIN(eof_p);

GL_BUILTIN(flush);

// Bitwise operations

GL_BUILTIN(logand);

GL_BUILTIN(logior);

GL_BUILTIN(logxor);

GL_BUILTIN(lognot);
