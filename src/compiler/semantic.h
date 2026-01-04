#ifndef MINI_LISP_X86_SRC_COMPILER_SEMANTIC_H_
#define MINI_LISP_X86_SRC_COMPILER_SEMANTIC_H_

#include "ast.h"
#include "builtin_hash.h"
#include "hashmap.h"
#include "../util/error.h"

enum sym_kind {
    SYM_VAR,
    SYM_FUNC,
    SYM_SPECIAL,
    SYM_BUILTIN
};

struct sym_info {
    enum sym_kind kind;
    int min_arity;
    int max_arity;  // -1 for variadic
};

struct env {
    struct u32hashmap *bindings;
    struct env *parent;
};

int analyze_program(struct ast_node *program, struct error_ctx *ctx);

#endif
