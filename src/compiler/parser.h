#ifndef MINI_LISP_X86_SRC_COMPILER_PARSER_H_
#define MINI_LISP_X86_SRC_COMPILER_PARSER_H_

void token_array_init(struct token_array *ta);
void token_array_append(struct token_array *ta, struct token t);
void token_array_free(struct token_array *ta);

#endif
