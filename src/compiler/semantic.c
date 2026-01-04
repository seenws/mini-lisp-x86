/*
    mini-lisp-x86 - A compiler for a subset of Common Lisp to x86_64
    Copyright (C) 2025 BolvarsDad

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

    File: semantic.c
    Purpose: Implements semantic analysis of target lisp program.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "hashmap.h"
#include "../util/error.h"

static struct sym_info *sym_info_new(enum sym_kind, int, int);
static struct env *env_new(struct env *);
static void env_free(struct env *);
static struct sym_info *env_lookup(struct env *, const char *);
static int env_insert(struct env *, const char *, struct sym_info *);

static void analyze_node(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_list(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_symbol(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_if(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_format(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_arithmetic(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_function_call(struct ast_node *, struct env *, struct error_ctx *ctc);
static void init_global_env();

static struct env *global_env = NULL;

static struct sym_info *
sym_info_new(enum sym_kind kind, int min_arity, int max_arity)
{
    struct sym_info *info = malloc(sizeof(*info));
    if (info == NULL)
        return NULL;

    info->kind = kind;
    info->min_arity = min_arity;
    info->max_arity = max_arity;

    return info;
}

static struct env *
env_new(struct env *parent)
{
    struct env *env = malloc(sizeof(*env));
    if (env == NULL)
        return NULL;

    env->bindings = hashmap_new(16);  // default 16 buckets
    if (env->bindings == NULL) {
        fprintf(stderr, "Unable to create bindings.\n");
        free(env);
        return NULL;
    }

    env->parent = parent;

    return env;
}

static void
env_free(struct env *env)
{
    if (env == NULL)
        return;

    hashmap_free(env->bindings);
    free(env);
}

static struct sym_info *
env_lookup(struct env *env, const char *key)
{
    if (env == NULL || key == NULL)
        return NULL;

    for (struct env *current = env; current != NULL; current = current->parent) {
        void *val = NULL;
        if (hashmap_lookup(current->bindings, key, &val) == 1) {
            return (struct sym_info *)val;
        }
    }

    return NULL;  // Not found
}

static int
env_insert(struct env *env, const char *key, struct sym_info *info)
{
    if (env == NULL || key == NULL || info == NULL)
        return -1;

    if (hashmap_insert(env->bindings, key, info) != 0)
        return -1;

    return 0;
}

static void
init_global_env(void)
{
    global_env = env_new(NULL);
    if (global_env == NULL)
        return;

    // Globals like t and nil (self-evaluating symbols)
    env_insert(global_env, "t", sym_info_new(SYM_VAR, 0, 0));
    env_insert(global_env, "nil", sym_info_new(SYM_VAR, 0, 0));

    env_insert(global_env, "+", sym_info_new(SYM_FUNC, 2, -1));
    env_insert(global_env, "-", sym_info_new(SYM_FUNC, 2, -1));
    env_insert(global_env, "*", sym_info_new(SYM_FUNC, 2, -1));
    env_insert(global_env, "/", sym_info_new(SYM_FUNC, 2, -1));
}

static void
analyze_list(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    if (node->as.list.count == 0)
        return;

    struct ast_node *op = node->as.list.children[0];
    if (op->type != NODE_SYMBOL) {
        error_ctx_push(ctx, ERR_ERROR, "Form must begin with a symbol\n");
        return;
    }

    enum builtin_type kind = builtin_kind(op->as.symbol, strlen(op->as.symbol));

    switch (kind) {
        case BUILTIN_PLUS:
        case BUILTIN_MINUS:
        case BUILTIN_MUL:
        case BUILTIN_DIV:
            analyze_arithmetic(node, env, ctx);
            break;

        case BUILTIN_IF:
            analyze_if(node, env, ctx);
            break;

        case BUILTIN_FORMAT:
            analyze_format(node, env, ctx);
            break;

        case BUILTIN_NOT:
        default:
            // User-defined
            struct sym_info *info = env_lookup(env, op->as.symbol);
            if (!info || info->kind != SYM_FUNC) {
                error_ctx_push(ctx, ERR_ERROR, "Error: Undefined function '%s'\n", op->as.symbol);
                return;
            }

            analyze_function_call(node, env, ctx);
            break;
    }
}

static void
analyze_node(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
        case NODE_LIST:
            analyze_list(node, env, ctx);
            break;

        case NODE_SYMBOL:
            analyze_symbol(node, env, ctx);
            break;

        case NODE_STRING:
        case NODE_NUMBER:
            // Self-evaluating
            break;

        default:
            error_ctx_push(ctx, ERR_ERROR, "Error: Unknown node type '%s'\n", node->as.symbol);
            break;
    }
}

static void
analyze_symbol(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    struct sym_info *info = env_lookup(env, node->as.symbol);
    if (!info) {
        error_ctx_push(ctx, ERR_ERROR, "Error: Undefined variable '%s'", node->as.symbol);
        return;
    }
}

static void
analyze_if(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2 || nargs > 3) {
        error_ctx_push(ctx, ERR_ERROR, "Error: Invalid number of arguments passed to 'if'\n");
        return;
    }

    analyze_node(node->as.list.children[1], env, ctx);  // test
    analyze_node(node->as.list.children[2], env, ctx);  // then

    if (nargs == 3)
        analyze_node(node->as.list.children[3], env, ctx);  // else
}

static void
analyze_format(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2) {
        error_ctx_push(ctx, ERR_ERROR, "Error: Invalid number of arguments passed to 'format'\n");
        printf("DEBUG: Push attempted for '%s'\n", node->as.symbol);
    }

    for (size_t i = 1; i < node->as.list.count; ++i)
        analyze_node(node->as.list.children[i], env, ctx);
}

static void
analyze_arithmetic(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2) {
        error_ctx_push(ctx, ERR_ERROR, "Error: Arithmetic operation '%s' expects two arguments.\n", node->as.symbol);
        return;
    }

    for (size_t i = 1; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], env, ctx);
    }
}

static void
analyze_function_call(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;

    struct ast_node *op = node->as.list.children[0];
    struct sym_info *info = env_lookup(env, op->as.symbol);  // Should exist

    if (nargs < (size_t)info->min_arity || (info->max_arity != -1 && nargs > (size_t)info->max_arity)) {
        error_ctx_push(ctx, ERR_ERROR, "Error: Invalid number of arguments passed to '%s'\n", node->as.symbol);
        return;
    }

    for (size_t i = 1; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], env, ctx);
    }
}

int
analyze_program(struct ast_node *program, struct error_ctx *ctx)
{
    init_global_env();
    analyze_node(program, global_env, ctx);

    return ctx->count == 0;
}
