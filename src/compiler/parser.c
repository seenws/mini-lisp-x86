/*
    mini-lisp-x86 - A compiler for a subset of Common Lisp to x86_64
    Copyright (C) 2025 BolvarsDad

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
#include <string.h>

#include "parser.h"

#include "../util/mlispc_strndup.h"

struct ast_node *
parse_list(struct token_stream *ts)
{
    struct ast_node *list = ast_list();

    while (!is_eof(ts) && peek_token(ts).type != TOK_RPAREN) {
        struct ast_node *expr = parse_expression(ts);
        ast_list_append(list, expr);
    }

    if (is_eof(ts)) {
        fprintf(stderr, "Error: Unexpected EOF while parsing list\n");
        exit(1);  // TODO: Better error handling
    }

    consume_token(ts);  // Eat the RPAREN
    return list;
}

struct ast_node *
parse_expression(struct token_stream *ts)
{
    if (is_eof(ts)) {
        fprintf(stderr, "Error: Unexpected EOF in expression\n");
        return NULL;
    }

    struct token t = peek_token(ts);

    if (t.type == TOK_LPAREN) {
        consume_token(ts);
        return parse_list(ts);
    }

    // For all other cases (atoms), consume and process
    t = consume_token(ts);

    switch (t.type) {
        case TOK_FUNCTION:
        case TOK_SYMBOL: {
            char *sym = mlispc_strndup(t.lexeme, t.len);
            return ast_symbol(sym);
        }

        case TOK_NUMERIC: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.*s", (int)t.len, t.lexeme);
            long num = strtol(buf, NULL, 10);
            return ast_number(num);
        }

        case TOK_STRING: {
            char *str = mlispc_strndup(t.lexeme, t.len);
            return ast_string(str);
        }

        default:
            fprintf(stderr, "Error: Unexpected token type %d (lexeme: '%.*s')\n", t.type, (int)t.len, t.lexeme);
            exit(1);
    }
}
