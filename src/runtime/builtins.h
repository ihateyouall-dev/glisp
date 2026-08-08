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
