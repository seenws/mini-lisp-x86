#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "hashmap.h"

static struct sym_info *sym_info_new(enum sym_kind, int, int);
static struct env *env_new(struct env *);
static void env_free(struct env *);
static struct sym_info *env_lookup(struct env *, const char *);
static int env_insert(struct env *, const char *, struct sym_info *);

static void analyze_node(struct ast_node *, struct env *);
static void analyze_list(struct ast_node *, struct env *);
static void analyze_symbol(struct ast_node *, struct env *);
static void analyze_if(struct ast_node *, struct env *);
static void analyze_format(struct ast_node *, struct env *);
static void analyze_arithmetic(struct ast_node *, struct env *);
static void analyze_function_call(struct ast_node *, struct env *);
static void init_global_env(void);

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

    env->bindings = hashmap_new(16);  // Assuming your impl takes initial size
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

    // TODO: Iterate hashmap to free sym_info * values if owned
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
        if (hashmap_lookup(current->bindings, key, &val) == 1) {  // Assuming 1 = found
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

    if (hashmap_insert(env->bindings, key, info) != 0)  // Assuming 0 = success
        return -1;

    return 0;
}

static void
init_global_env(void)
{
    global_env = env_new(NULL);

    // Globals like t and nil (self-evaluating symbols)
    env_insert(global_env, "t", sym_info_new(SYM_VAR, 0, 0));
    env_insert(global_env, "nil", sym_info_new(SYM_VAR, 0, 0));

    // Built-ins as SYM_FUNC (add SYM_BUILTIN to enum if you want separate)
    env_insert(global_env, "+", sym_info_new(SYM_FUNC, 2, -1));  // Variadic, min 2
    env_insert(global_env, "-", sym_info_new(SYM_FUNC, 2, -1));
    // Add *, /, etc.
}

static void
analyze_list(struct ast_node *node, struct env *env)
{
    if (node->as.list.count == 0)
        return;

    struct ast_node *op = node->as.list.children[0];
    if (op->type != NODE_SYMBOL) {
        fprintf(stderr, "Analysis error: Form must begin with a symbol.\n");
        exit(1);
    }

    enum builtin_type kind = builtin_kind(op->as.symbol, strlen(op->as.symbol));

    switch (kind) {
        case BUILTIN_PLUS:
        case BUILTIN_MINUS:
        case BUILTIN_MUL:
        case BUILTIN_DIV:
            analyze_arithmetic(node, env);
            break;

        case BUILTIN_IF:
            analyze_if(node, env);
            break;

        case BUILTIN_FORMAT:
            analyze_format(node, env);
            break;

        case BUILTIN_NOT:
            // User-defined
            struct sym_info *info = env_lookup(env, op->as.symbol);
            if (!info || info->kind != SYM_FUNC) {
                fprintf(stderr, "Error: Undefined function '%s'\n", op->as.symbol);
                exit(1);
            }

            analyze_function_call(node, env);
            break;

        default:
            // Unknown builtin or error
            fprintf(stderr, "Error: Unknown operator '%s'\n", op->as.symbol);
            exit(1);
    }
}

static void
analyze_node(struct ast_node *node, struct env *env)
{
    if (node == NULL)
        return;

    switch (node->type) {
        case NODE_LIST:
            analyze_list(node, env);
            break;

        case NODE_SYMBOL:
            analyze_symbol(node, env);
            break;

        case NODE_STRING:
        case NODE_NUMBER:
            // Self-evaluating
            break;

        default:
            fprintf(stderr, "Unknown node type encountered\n");
            exit(1);
    }
}

static void
analyze_symbol(struct ast_node *node, struct env *env)
{
    struct sym_info *info = env_lookup(env, node->as.symbol);
    if (!info) {
        fprintf(stderr, "Error: unhandled unbound symbol '%s'\n", node->as.symbol);
        exit(1);
    }
}

static void
analyze_if(struct ast_node *node, struct env *env)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2 || nargs > 3) {
        fprintf(stderr, "Error: 'if' construct expects at least 2 and at most 3 arguments.\n");
        exit(1);
    }

    analyze_node(node->as.list.children[1], env);  // test
    analyze_node(node->as.list.children[2], env);  // then

    if (nargs == 3)
        analyze_node(node->as.list.children[3], env);  // else
}

static void
analyze_format(struct ast_node *node, struct env *env)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2) {
        fprintf(stderr, "Error: 'format' construct expects at least 2 arguments.\n");
        exit(1);
    }

    // Analyze args (start at 1, skip op)
    for (size_t i = 1; i < node->as.list.count; ++i)
        analyze_node(node->as.list.children[i], env);
}

static void
analyze_arithmetic(struct ast_node *node, struct env *env)
{
    size_t nargs = node->as.list.count - 1;
    if (nargs < 2) {
        fprintf(stderr, "Error: Arithmetic op expects at least 2 arguments.\n");
        exit(1);
    }

    // Analyze each arg (could be expressions, not just literals)
    for (size_t i = 1; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], env);
        // Optional: Later add type check if arg evaluates to number
    }
}

static void
analyze_function_call(struct ast_node *node, struct env *env)
{
    size_t nargs = node->as.list.count - 1;

    // Get info from op (assumed already looked up)
    struct ast_node *op = node->as.list.children[0];
    struct sym_info *info = env_lookup(env, op->as.symbol);  // Should exist

    // Arity check
    if (nargs < (size_t)info->min_arity || (info->max_arity != -1 && nargs > (size_t)info->max_arity)) {
        fprintf(stderr, "Error: Wrong number of args for '%s' (got %zu, expected %d-%d)\n",
                op->as.symbol, nargs, info->min_arity, info->max_arity);
        exit(1);
    }

    // Analyze args
    for (size_t i = 1; i < node->as.list.count; ++i) {
        analyze_node(node->as.list.children[i], env);
    }
}

void
analyze_program(struct ast_node *program)
{
    if (global_env == NULL)
        init_global_env();

    analyze_node(program, global_env);
}
