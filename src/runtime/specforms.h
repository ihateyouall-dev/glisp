#pragma once

#include "value.h"

typedef struct gl_env_t gl_env_t;

#define GL_SPECIAL_FORM(Name) gl_value_t *gl_specform_##Name(gl_env_t *env, gl_ast_node_t *args)

GL_SPECIAL_FORM(if);

GL_SPECIAL_FORM(defun);

GL_SPECIAL_FORM(set);

GL_SPECIAL_FORM(progn);
