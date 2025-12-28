#include <stdio.h>
#include <stdlib.h>

#include "token_array.h"

void
token_array_init(struct token_array *ta)
{
    ta->count = 0;
    ta->capacity = 256;
    ta->tokens = malloc(ta->capacity * sizeof(struct token));
    if (!ta->tokens) {
        perror("malloc failed in token_array_init");
        exit(1);
    }
}

void
token_array_append(struct token_array *ta, struct token t)
{
    if (ta->count >= ta->capacity) {
        ta->capacity *= 2;
        ta->tokens = realloc(ta->tokens, ta->capacity * sizeof(struct token));
        if (!ta->tokens) {
            perror("realloc failed in token_array_append");
            exit(1);
        }
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
