#pragma once

#include "value.h"

#define GL_BUILTIN(Name) gl_value_t *gl_builtin_##Name(gl_value_array_t *args)

GL_BUILTIN(exit);

GL_BUILTIN(print);

GL_BUILTIN(println);

GL_BUILTIN(car);

GL_BUILTIN(cdr);

GL_BUILTIN(add);

GL_BUILTIN(sub);

GL_BUILTIN(mul);
