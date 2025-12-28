#include <stdio.h>
#include <stdlib.h>

#include "token_stream.h"

struct token_stream
token_stream_create(struct token_array *array)
{
    struct token_stream *ts = {0};

    ts.array = array;
    ts.pos = 0;

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
