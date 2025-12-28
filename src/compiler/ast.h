#ifndef MINI_LISP_X86_SRC_COMPILER_AST_H_
#define MINI_LISP_X86_SRC_COMPILER_AST_H_

#include "lexer.h"

enum node_type {
    NODE_SYMBOL,
    NODE_STRING,
    NODE_NUMBER,
    NODE_LIST,      // (a b c)
    NODE_NIL        // ()
};

struct ast_node {
    enum node_type type;

    union {
        char *symbol;
        char *string;
        long number;

        struct {
            struct ast_node **children;
            size_t count;
            size_t capacity;
        } list;
    } as;
};

struct ast_node *ast_symbol(const char *name);
struct ast_node *ast_string(const char *value);
struct ast_node *ast_number(long value);
struct ast_node *ast_list(void);
void   ast_list_append(struct ast_node *list, struct ast_node *node);

struct ast_node *ast_node_new(enum node_type type);
void   ast_node_free(struct ast_node *node);
void   ast_node_print(struct ast_node *node, int indent);

#endif
