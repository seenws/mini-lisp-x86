#ifndef MINI_LISP_X86_SRC_COMPILER_PARSER_H_
#define MINI_LISP_X86_SRC_COMPILER_PARSER_H_

#include "ast.h"
#include "token_array.h"
#include "token_stream.h"
#include "../util/error.h"

/* Syntax errors are reported through ctx; parsing stops at the first
 * error and returns NULL (parse_program returns the forms parsed so
 * far -- callers must check ctx->count before using the AST). */
struct ast_node *parse_list(struct token_stream *ts, struct error_ctx *ctx, size_t line, size_t col);
struct ast_node *parse_expression(struct token_stream *ts, struct error_ctx *ctx);
struct ast_node *parse_program(struct token_stream *ts, struct error_ctx *ctx);

#endif
