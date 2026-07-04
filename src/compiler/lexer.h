#ifndef MINI_LISP_X86_COMPILER_LEXER_H_
#define MINI_LISP_X86_COMPILER_LEXER_H_

#include <stddef.h>

void            init_token_handlers (void);
struct lexer    lexer_create        (char const*src, size_t srclen);
struct token    lexer_next          (struct lexer *l);
char            lexer_peek          (struct lexer *l);
void            tokenize            (char const *line, size_t len);
struct token    lex_paren           (struct lexer *l);
struct token    lex_string          (struct lexer *l);
struct token    lex_function        (struct lexer *l);
struct token    lex_numeric         (struct lexer *l);
struct token    lex_macro           (struct lexer *l);
struct token    lex_keyword         (struct lexer *l);
struct token    lex_symbol          (struct lexer *l);
struct token    lex_comment         (struct lexer *l);
int             is_symbol_char      (char c);

enum toktype {
    TOK_END = 0,
    TOK_INVALID,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_SYMBOL,     // e.g. variable names
    TOK_KEYWORD,    // e.g. 'format'
    TOK_STRING,
    TOK_FUNCTION,
    TOK_MACRO,
    TOK_NUMERIC,
    TOK_COMMENT,
};

struct token {
    char const     *lexeme;
    size_t          len;
    enum toktype    type;
    size_t          line;   // 1-based
    size_t          col;    // 1-based
};

struct lexer {
    char const     *content;
    size_t          len;
    size_t          cursor;
    size_t          line;       // 1-based, line of the current cursor
    size_t          line_start; // byte offset where the current line begins
};

char const *token_to_str(enum toktype type);

#endif /* MINI_LISP_X86_COMPILER_LEXER_H_ */
