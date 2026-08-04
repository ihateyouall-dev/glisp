#pragma once

#include "env.h"

#define GL_SPECIAL_FORM(Name) gl_value_t *gl_specform_##Name(gl_env_t *env, gl_ast_node_t *args)

GL_SPECIAL_FORM(if);

GL_SPECIAL_FORM(defun);
