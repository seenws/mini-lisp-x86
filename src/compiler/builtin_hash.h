#ifndef MINI_LISP_X86_SRC_COMPILER_BUILTIN_HASH_H_
#define MINI_LISP_X86_SRC_COMPILER_BUILTIN_HASH_H_

#include <stddef.h>
#include <string.h>

enum builtin_type {
    BUILTIN_NOT,
    BUILTIN_PLUS,
    BUILTIN_MINUS,
    BUILTIN_MUL,
    BUILTIN_DIV,
    BUILTIN_IF,
    BUILTIN_LET,
    BUILTIN_QUOTE,
    BUILTIN_LAMBDA,
    BUILTIN_DEFUN,
    BUILTIN_FORMAT
};

const char *lookup_builtin(const char *str, size_t len);

// Convert matched string to our enum
static inline enum builtin_type builtin_kind(const char *str, size_t len)
{
    const char *matched = lookup_builtin(str, len);
    if (matched == NULL) {
        return BUILTIN_NOT;
    }

    // Fast path for single-char operators
    if (len == 1) {
        switch (str[0]) {
            case '+': return BUILTIN_PLUS;
            case '-': return BUILTIN_MINUS;
            case '*': return BUILTIN_MUL;
            case '/': return BUILTIN_DIV;
        }
    }

    // Longer names
    if (strcmp(matched, "if") == 0)     return BUILTIN_IF;
    if (strcmp(matched, "let") == 0)    return BUILTIN_LET;
    if (strcmp(matched, "quote") == 0)  return BUILTIN_QUOTE;
    if (strcmp(matched, "lambda") == 0) return BUILTIN_LAMBDA;
    if (strcmp(matched, "defun") == 0)  return BUILTIN_DEFUN;
    if (strcmp(matched, "format") == 0) return BUILTIN_FORMAT;

    return BUILTIN_NOT;  // Should never happen
}

#endif
