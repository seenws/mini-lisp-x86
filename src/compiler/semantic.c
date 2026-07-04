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

    File: semantic.c
    Purpose: Implements semantic analysis of target lisp program.
*/

#include <assert.h>
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
static void analyze_function_call(struct ast_node *, struct env *, struct error_ctx *ctx);
static void analyze_let(struct ast_node *, struct env *, struct error_ctx *ctx);
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

    // hashmap_free releases buckets and keys but not values;
    // the sym_info values belong to us
    for (size_t i = 0; i < env->bindings->nbuckets; ++i)
        for (struct u32bucket *b = env->bindings->buckets[i]; b != NULL; b = b->next)
            free(b->val);

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

// Looks only in the given scope, ignoring parents.
// Needed for duplicate-binding checks: shadowing an outer
// binding is legal, rebinding within the same scope is not.
static struct sym_info *
env_lookup_local(struct env *env, const char *key)
{
    void *val = NULL;

    if (env == NULL || key == NULL)
        return NULL;

    if (hashmap_lookup(env->bindings, key, &val) == 1)
        return (struct sym_info *)val;

    return NULL;
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
    env_insert(global_env, "format", sym_info_new(SYM_FUNC, 2, -1));
    env_insert(global_env, "let", sym_info_new(SYM_FUNC, 1, -1));
}

static void
analyze_list(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    if (node->as.list.count == 0)
        return;

    struct ast_node *op = node->as.list.children[0];
    if (op->type != NODE_SYMBOL) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "Form must begin with a symbol");
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

        case BUILTIN_LET:
            analyze_let(node, env, ctx);
            break;

        case BUILTIN_NOT:
        default: {
            // User-defined
            struct sym_info *info = env_lookup(env, op->as.symbol);
            if (!info || info->kind != SYM_FUNC) {
                error_ctx_push(ctx, ERR_ERROR, op->line, op->col, "Undefined function '%s'", op->as.symbol);
                return;
            }

            analyze_function_call(node, env, ctx);
            break;
        }
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
            error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "Unknown node type %d", node->type);
            break;
    }
}

// Checks if a symbol is present in the symbol table
static void
analyze_symbol(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    // Keywords (:foo) are self-evaluating, never looked up
    if (node->as.symbol[0] == ':')
        return;

    struct sym_info *info = env_lookup(env, node->as.symbol);
    if (!info) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "Undefined variable '%s' is unbound", node->as.symbol);
        return;
    }
}

static void
analyze_if(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2 || nargs > 3) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "Invalid number of arguments passed to 'if'");
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
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                "Invalid number of arguments passed to 'format' (expected a destination and a format string)");

        return;
    }

    // Destination must be the symbol `t` or `nil`
    struct ast_node *dest = node->as.list.children[1];
    if (dest->type != NODE_SYMBOL
            || (strcmp(dest->as.symbol, "t") != 0 && strcmp(dest->as.symbol, "nil") != 0)) {
        error_ctx_push(ctx, ERR_ERROR, dest->line, dest->col,
                "Invalid destination provided to function `format` (expected t or nil)");

        return;
    }

    // Destination is already validated; analyze the remaining arguments
    for (size_t i = 2; i < node->as.list.count; ++i)
        analyze_node(node->as.list.children[i], env, ctx);
}

static void
analyze_let(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 1) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "'let' expects at least one argument (bindings list)");
        return;
    }

    struct ast_node *bindings = node->as.list.children[1];
    if (bindings->type != NODE_LIST) {
        error_ctx_push(ctx, ERR_ERROR, bindings->line, bindings->col, "First argument to 'let' must be a list of bindings");
        return;
    }

    struct env *local_env = env_new(env);
    if (local_env == NULL) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col, "Failed to create local environment for 'let'");
        return;
    }

    for (size_t i = 0; i < bindings->as.list.count; ++i) {
        struct ast_node *binding = bindings->as.list.children[i];
        if (binding->type != NODE_LIST || binding->as.list.count != 2) {
            error_ctx_push(ctx, ERR_ERROR, binding->line, binding->col, "Each binding in 'let' must be a list of (symbol value)");
            env_free(local_env);
            return;
        }

        struct ast_node *var = binding->as.list.children[0];
        struct ast_node *val = binding->as.list.children[1];

        if (var->type != NODE_SYMBOL) {
            error_ctx_push(ctx, ERR_ERROR, var->line, var->col, "Binding variable must be a symbol");
            env_free(local_env);
            return;
        }

        // Only reject rebinding within this same let;
        // shadowing an outer scope's binding is legal
        if (env_lookup_local(local_env, var->as.symbol)) {
            error_ctx_push(ctx, ERR_ERROR, var->line, var->col, "Duplicate binding '%s' in 'let'", var->as.symbol);
            env_free(local_env);
            return;
        }

        analyze_node(val, env, ctx);

        struct sym_info *info = sym_info_new(SYM_VAR, 0, 0);
        if (!info || env_insert(local_env, var->as.symbol, info) != 0) {
            error_ctx_push(ctx, ERR_ERROR, var->line, var->col, "Failed to insert binding '%s'", var->as.symbol);
            if (info) free(info);
            env_free(local_env);
            return;
        }
    }

    for (size_t i = 2; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], local_env, ctx);
    }

    env_free(local_env);
}

static void
analyze_arithmetic(struct ast_node *node, struct env *env, struct error_ctx *ctx)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 1) {
        error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                "Arithmetic operation '%s' expects at least one argument",
                node->as.list.children[0]->as.symbol);

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
    struct sym_info *info = env_lookup(env, op->as.symbol);

    assert(info != NULL); // analyze_list verified this lookup before dispatching here

    if (nargs < (size_t)info->min_arity || (info->max_arity != -1 && nargs > (size_t)info->max_arity)) {
        if (info->max_arity == -1)
            error_ctx_push(ctx, ERR_ERROR, op->line, op->col,
                    "Invalid number of arguments passed to '%s' (expected at least %d, got %zu)",
                    op->as.symbol, info->min_arity, nargs);
        else
            error_ctx_push(ctx, ERR_ERROR, op->line, op->col,
                    "Invalid number of arguments passed to '%s' (expected %d-%d, got %zu)",
                    op->as.symbol, info->min_arity, info->max_arity, nargs);

        return;
    }

    for (size_t i = 1; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], env, ctx);
    }
}

// The root node produced by parse_program is a list of top-level
// forms, not itself a form -- analyze each child on its own.
int
analyze_program(struct ast_node *program, struct error_ctx *ctx)
{
    init_global_env();

    if (program == NULL)
        return 0;

    if (program->type == NODE_LIST) {
        for (size_t i = 0; i < program->as.list.count; ++i)
            analyze_node(program->as.list.children[i], global_env, ctx);
    } else {
        analyze_node(program, global_env, ctx);
    }

    return ctx->count == 0;
}
