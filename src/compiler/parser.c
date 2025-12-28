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

#include "ast.h"
#include "parser.h"
#include "token_stream.h"

void
token_array_init(struct token_array *ta)
{
    ta->count = 0;
    ta->capacity = 256;
    ta->tokens = malloc(ta->capacity * sizeof(struct token));
}

void
token_array_append(struct token_array *ta, struct token t)
{
    if (ta->count >= ta->capacity) {
        ta->capacity *= 2;
        ta->tokens = realloc(ta->tokens, ta->capacity * sizeof(struct token));
    }

    // lexeme points to source buffer
    ta->tokens[ta->count++] = t;
}

void
token_array_free(struct token_array *ta)
{
    free(ta->tokens);
    ta->tokens = NULL;
    ta->count = ta->capacity = 0;
}

struct ast_node *
parse_expression(struct token_stream *ts)
{
    struct ast_node_t
}
