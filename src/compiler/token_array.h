#ifndef MINI_LISP_X86_SRC_COMPILER_TOKEN_ARRAY_H_
#define MINI_LISP_X86_SRC_COMPILER_TOKEN_ARRAY_H_

#include "lexer.h"
#include <stddef.h>

struct token_array {
    struct token *tokens;
    size_t count;
    size_t capacity;
};

void token_array_init(struct token_array *ta);
void token_array_append(struct token_array *ta, struct token t);
void token_array_free(struct token_array *ta);

#endif
