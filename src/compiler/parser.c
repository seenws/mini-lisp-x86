/*
    mini-lisp-x86 - A compiler for a subset of Common Lisp to x86_64
    Copyright (C) 2025 Sinan Olsson-Pasic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. 

    File: parser.c
    Purpose: Utilizes the data structure found in ast.h and ast.c to parse the input file tokens
    retrieved from lexer.c and lexer.h into an Annotated Abstract Syntax Tree (AST).
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "parser.h"

#include "../util/mlispc_strndup.h"

// line/col identify the opening '(' so an unterminated list is
// reported where it starts, not where the input ran out
struct ast_node *
parse_list(struct token_stream *ts, struct error_ctx *ctx, size_t line, size_t col)
{
    struct ast_node *list = ast_list();

    while (!is_eof(ts) && peek_token(ts).type != TOK_RPAREN) {
        struct ast_node *expr = parse_expression(ts, ctx);
        if (expr == NULL) { // error already reported below us
            ast_node_free(list);
            return NULL;
        }

        ast_list_append(list, expr);
    }

    if (is_eof(ts)) {
        error_ctx_push(ctx, ERR_ERROR, line, col,
                "Unterminated list; expected ')' before end of input");
        ast_node_free(list);
        return NULL;
    }

    consume_token(ts);  // Eat the RPAREN
    return list;
}

struct ast_node *
parse_expression(struct token_stream *ts, struct error_ctx *ctx)
{
    if (is_eof(ts)) {
        error_ctx_push(ctx, ERR_ERROR, 0, 0, "Unexpected end of input in expression");
        return NULL;
    }

    struct token t = peek_token(ts);
    struct ast_node *node = NULL;

    if (t.type == TOK_LPAREN) {
        consume_token(ts);
        node = parse_list(ts, ctx, t.line, t.col);

        if (node != NULL) {
            node->line = t.line;
            node->col  = t.col;
        }

        return node;
    }

    // For all other cases (atoms), consume and process
    t = consume_token(ts);

    switch (t.type) {
        case TOK_FUNCTION:
        case TOK_KEYWORD:
        case TOK_SYMBOL: {
            char *sym = mlispc_strndup(t.lexeme, t.len);
            node = ast_symbol(sym);
            free(sym);  // ast_symbol keeps its own copy
            
            break;
        }

        case TOK_NUMERIC: {
            char buf[32];
            if (t.len >= sizeof(buf)) {
                error_ctx_push(ctx, ERR_ERROR, t.line, t.col,
                        "Numeric literal '%.*s' is too long", (int)t.len, t.lexeme);
                return NULL;
            }

            snprintf(buf, sizeof(buf), "%.*s", (int)t.len, t.lexeme);

            char *end;
            errno = 0;
            long num = strtol(buf, &end, 10);

            if (errno == ERANGE) {
                error_ctx_push(ctx, ERR_ERROR, t.line, t.col,
                        "Integer literal '%s' is out of range", buf);
                return NULL;
            }

            // lex_numeric also accepts '.' and '/' (floats, rationals);
            // reject whatever strtol could not consume until those exist
            if (*end != '\0') {
                error_ctx_push(ctx, ERR_ERROR, t.line, t.col,
                        "Unsupported numeric literal '%s' (only integers are implemented)", buf);
                return NULL;
            }

            node = ast_number(num);

            break;
        }

        case TOK_STRING: {
            char *str = mlispc_strndup(t.lexeme, t.len);
            node = ast_string(str);
            free(str); // ast_string keeps its own copy
            
            break;
        }

        default:
            error_ctx_push(ctx, ERR_ERROR, t.line, t.col,
                    "Unexpected token %s ('%.*s')",
                    token_to_str(t.type), (int)t.len, t.lexeme);
            return NULL;
    }

    if (node != NULL) {
        node->line = t.line;
        node->col  = t.col;
    }

    return node;
}

struct ast_node *
parse_program(struct token_stream *ts, struct error_ctx *ctx)
{
    struct ast_node *program = ast_list();

    while (!is_eof(ts)) {
        struct ast_node *expr = parse_expression(ts, ctx);
        if (expr == NULL)
            break; // error already reported; stop at the first syntax error

        ast_list_append(program, expr);
    }

    return program;
}
