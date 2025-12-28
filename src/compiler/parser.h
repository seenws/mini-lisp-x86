#ifndef MINI_LISP_X86_SRC_COMPILER_PARSER_H_
#define MINI_LISP_X86_SRC_COMPILER_PARSER_H_

#include "ast.h"           // For ast_node_t
#include "token_array.h"   // For struct token_array (via includes)
#include "token_stream.h"  // For struct token_stream

struct ast_node *parse_expression(struct token_stream *ts);
// Add more parser functions here as you build them, e.g., parse_list

#endif
