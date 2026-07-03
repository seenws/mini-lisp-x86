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

    File: token_stream.c
    Purpose: Implements the token stream used for semantic analysis
*/

#include <stdio.h>
#include <stdlib.h>

#include "token_stream.h"

struct token_stream *
token_stream_create(struct token_array *array)
{
    struct token_stream *ts = malloc(sizeof *ts);
    if (!ts) return NULL;

    ts->array = array;
    ts->pos = 0;

    return ts;
}

struct token
peek_token(struct token_stream *ts)
{
    if (ts->pos >= ts->array->count) {
        struct token eof = { .type = TOK_END, .lexeme = NULL, .len = 0 };
        return eof;
    }

    return ts->array->tokens[ts->pos];
}

struct token
consume_token(struct token_stream *ts)
{
    if (ts->pos >= ts->array->count) {
        struct token eof = { .type = TOK_END, .lexeme = NULL, .len = 0 };
        return eof;
    }
    
    return ts->array->tokens[ts->pos++];
}

int
is_eof(struct token_stream *ts)
{
    return peek_token(ts).type == TOK_END;
}
