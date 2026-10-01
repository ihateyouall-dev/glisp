#pragma once

#include "value.h"

typedef struct gl_env_t gl_env_t;

#define GL_SPECIAL_FORM(Name) gl_value_t *gl_specform_##Name(gl_env_t *env, gl_ast_node_t *args)

GL_SPECIAL_FORM(if);

GL_SPECIAL_FORM(defun);

// Same as defun, but does not define function in environment
GL_SPECIAL_FORM(lambda);

GL_SPECIAL_FORM(set);

GL_SPECIAL_FORM(set_global);

GL_SPECIAL_FORM(progn);

GL_SPECIAL_FORM(while);

GL_SPECIAL_FORM(quote);

GL_SPECIAL_FORM(function);

GL_SPECIAL_FORM(let);

GL_SPECIAL_FORM(let_star);

GL_SPECIAL_FORM(pragma);

GL_SPECIAL_FORM(cond);

GL_SPECIAL_FORM(for);

GL_SPECIAL_FORM(and);

GL_SPECIAL_FORM(or);

GL_SPECIAL_FORM(add_to_list);

GL_SPECIAL_FORM(defconst);
