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

    File: ast.c
    Purpose: Implements the AST (Abstract Sytax Tree) for x86_64 code generation.
*/
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"
#include "../util/mlispc_strdup.h"

struct ast_node *
ast_node_create(enum node_type type)
{
    struct ast_node *n = malloc(sizeof *n);
    if (!n) return NULL;

    n->type = type;
    n->line = 0;
    n->col  = 0;

    if (type == NODE_LIST) {
        n->as.list.capacity = 16;
        n->as.list.count = 0;
        n->as.list.children =
            calloc(n->as.list.capacity, sizeof *n->as.list.children);

        if (!n->as.list.children) {
            free(n);
            return NULL;
        }
    }

    return n;
}

struct ast_node *
ast_symbol(const char *name)
{
    struct ast_node *n = ast_node_create(NODE_SYMBOL);

    n->as.symbol = mlispc_strdup(name);

    return n;
}

struct ast_node *
ast_string(const char *value)
{
    struct ast_node *n = ast_node_create(NODE_STRING);
    n->as.string = mlispc_strdup(value);
    return n;
}

struct ast_node *
ast_number(long value)
{
    struct ast_node *n = ast_node_create(NODE_NUMBER);
    n->as.number = value;

    return n;
}

struct ast_node *
ast_list(void)
{
    return ast_node_create(NODE_LIST);
}

void
ast_node_free(struct ast_node *node)
{
    if (node == NULL)
        return;

    switch (node->type) {
        case NODE_SYMBOL:
            free(node->as.symbol);
            break;

        case NODE_STRING:
            free(node->as.string);
            break;

        case NODE_LIST:
            for (size_t i = 0; i < node->as.list.count; ++i)
                ast_node_free(node->as.list.children[i]);
            free(node->as.list.children);
            break;

        default:
            break;
    }

    free(node);
}

void
ast_list_append(struct ast_node *list, struct ast_node *node)
{
    if (list->type != NODE_LIST) return;

    if (list->as.list.count == list->as.list.capacity) {
        list->as.list.capacity *= 2;
        list->as.list.children = realloc(
                list->as.list.children,
                list->as.list.capacity * sizeof *list->as.list.children
        );
    }

    list->as.list.children[list->as.list.count++] = node;
}
