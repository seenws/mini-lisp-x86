#ifndef MINI_LISP_X86_SRC_COMPILER_PARSER_H_
#define MINI_LISP_X86_SRC_COMPILER_PARSER_H_

#include "ast.h"
#include "token_array.h"
#include "token_stream.h"

struct ast_node *parse_list(struct token_stream *ts);
struct ast_node *parse_expression(struct token_stream *ts);

#endif
