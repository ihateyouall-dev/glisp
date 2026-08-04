#pragma once

#include "ast.h"
#include "env.h"
#include "value.h"

gl_value_t *gl_eval(gl_ast_node_t *node, gl_env_t *env);

gl_value_t *gl_parse_and_eval(const char *restrict src, gl_env_t *env);
