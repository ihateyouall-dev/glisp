#pragma once

#include "value.h"

#define GL_BUILTIN(Name) gl_value_t *gl_builtin_##Name(gl_env_t *env, gl_value_array_t *args)

GL_BUILTIN(exit);

GL_BUILTIN(print);

GL_BUILTIN(add);

GL_BUILTIN(sub);
