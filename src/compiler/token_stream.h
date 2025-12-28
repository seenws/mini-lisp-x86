#ifndef MINI_LISP_X86_SRC_COMPILER_TOKEN_STREAM_H_
#define MINI_LISP_X86_SRC_COMPILER_TOKEN_STREAM_H_

#include "token_array.h"

struct token_stream {
    struct token_array *array;
    size_t pos; // current position (index into array->tokens)
};

struct token_stream    *token_stream_create(struct token_array *array);
struct token            peek_token(struct token_stream *ts);
struct token            consume_token(struct token_stream *ts);
int is_eof              (struct token_stream *ts);

#endif
