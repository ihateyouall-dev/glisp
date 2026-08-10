#pragma once

#include "ast.h"
#include "env.h"
#include "parser.h"
#include "value.h"

gl_value_t *gl_eval(gl_ast_node_t *node, gl_env_t *env);

gl_value_t *gl_parse_and_eval(gl_parser_t *restrict parser, gl_env_t *env);
