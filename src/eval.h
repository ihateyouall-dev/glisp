#pragma once

#include "ast.h"
#include "runtime/env.h"

gl_value_t *gl_eval(gl_ast_node_t *node, gl_env_t *env);
