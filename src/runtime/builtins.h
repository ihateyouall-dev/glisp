#pragma once

#include "value.h"

#define GL_BUILTIN(Name) gl_value_t *gl_builtin_##Name(gl_value_array_t *args)

GL_BUILTIN(exit);

GL_BUILTIN(print);

GL_BUILTIN(println);

GL_BUILTIN(car);

GL_BUILTIN(cdr);

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

GL_BUILTIN(not );

GL_BUILTIN(and);

GL_BUILTIN(or);

// Return symbol representing type of arg
GL_BUILTIN(typeof);

GL_BUILTIN(int_p);

GL_BUILTIN(float_p);

GL_BUILTIN(number_p);

GL_BUILTIN(symbol_p);

GL_BUILTIN(nil_p);

GL_BUILTIN(function_p);

GL_BUILTIN(list_p);
