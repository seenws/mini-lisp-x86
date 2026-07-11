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

    File: ir.c
    Purpose: Implements the three-address-code (TAC) program buffer and
    AST-to-IR translation.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "ast.h"
#include "builtin_hash.h"
#include "hashmap.h"
#include "../util/mlispc_strdup.h"

#define IR_INITIAL_CAPACITY 1024 // instructions / strings before first resize

/*
 * Translation-time scopes: a let binding maps its name to the temp
 * already holding the init expression's value, so a symbol reference
 * resolves to that temp and emits nothing. This identification of
 * variable and temp relies on bindings being immutable (the language
 * has no setq); mutation or closures will need real storage.
 */
struct ir_scope {
    struct u32hashmap *bindings; // name -> temp, stored as (void *)(temp + 1)
    struct ir_scope *parent;
};

static int translate_expr(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope);
static int translate_arithmetic(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope, enum builtin_type kind);
static int translate_let(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope);
static int translate_if(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope);
static int translate_nil(struct ir_program *p);

struct ir_program *
ir_program_new(void)
{
    struct ir_program *p = malloc(sizeof(struct ir_program));
    if (p == NULL)
        return NULL;

    p->instructions = malloc(IR_INITIAL_CAPACITY * sizeof(struct ir_instr));
    p->count = 0;
    p->capacity = IR_INITIAL_CAPACITY;

    p->strings = malloc(IR_INITIAL_CAPACITY * sizeof(char *));
    p->str_count = 0;
    p->str_capacity = IR_INITIAL_CAPACITY;

    p->temp_count = 0;
    p->label_count = 0;

    if (p->instructions == NULL || p->strings == NULL) {
        ir_program_free(p); // allocation failure, abort
        return NULL;
    }

    return p;
}

void
ir_program_free(struct ir_program *p)
{
    if (p == NULL)
        return;

    free(p->instructions);

    for (size_t i = 0; i < p->str_count; ++i)
        free(p->strings[i]);

    free(p->strings);
    free(p);
}

int
ir_program_instr_resize(struct ir_program *p)
{
    if (p == NULL)
        return -1;

    size_t new_capacity = p->capacity * 2;
    if (new_capacity < p->capacity) // overflow check
        return -1;

    struct ir_instr *grown = realloc(p->instructions, new_capacity * sizeof(struct ir_instr));
    if (grown == NULL) {
        fprintf(stderr, "Fatal: Unable to `realloc` in ir_program_instr_resize\n");
        return -1;
    }

    p->instructions = grown;
    p->capacity = new_capacity;

    return 0;
}

int
ir_program_str_resize(struct ir_program *p)
{
    if (p == NULL)
        return -1;

    size_t new_str_capacity = p->str_capacity * 2;
    if (new_str_capacity < p->str_capacity)
        return -1;

    char **grown = realloc(p->strings, new_str_capacity * sizeof(char *));
    if (grown == NULL) {
        fprintf(stderr, "Fatal: Unable to `realloc` in ir_program_str_resize\n");
        return -1;
    }

    p->strings = grown;
    p->str_capacity = new_str_capacity;

    return 0;
}

int
ir_emit(struct ir_program *p, enum ir_ops op, int dst, int src1, int src2, int64_t imm)
{
    if (p == NULL)
        return -1;

    if (p->count >= p->capacity)
        if (ir_program_instr_resize(p) != 0)
            return -1; // resize failed, abort insert

    p->instructions[p->count].op = op;
    p->instructions[p->count].dst = dst;
    p->instructions[p->count].src1 = src1;
    p->instructions[p->count].src2 = src2;
    p->instructions[p->count].imm = imm;
    p->count++;

    return 0;
}

int64_t
ir_program_str_push(struct ir_program *p, const char *str)
{
    if (p == NULL)
        return -1;

    if (p->str_count >= p->str_capacity)
        if (ir_program_str_resize(p) != 0)
            return -1; // resize failed, abort insert

    p->strings[p->str_count] = mlispc_strdup(str);
    if (p->strings[p->str_count] == NULL)
        return -1;

    return (int64_t)p->str_count++;
}

int
ir_new_temp(struct ir_program *p)
{
    assert(p != NULL);

    return p->temp_count++;
}

int
ir_new_label(struct ir_program *p)
{
    assert(p != NULL);

    return p->label_count++;
}

static struct ir_scope *
ir_scope_new(struct ir_scope *parent)
{
    struct ir_scope *scope = malloc(sizeof(*scope));
    if (scope == NULL)
        return NULL;

    scope->bindings = hashmap_new(16); // default 16 buckets
    if (scope->bindings == NULL) {
        free(scope);
        return NULL;
    }

    scope->parent = parent;

    return scope;
}

static void
ir_scope_free(struct ir_scope *scope)
{
    if (scope == NULL)
        return;

    // values are encoded ints, nothing to free; the hashmap owns its keys
    hashmap_free(scope->bindings);
    free(scope);
}

// Returns the temp bound to name, or -1 if unbound in every enclosing scope.
static int
ir_scope_lookup(struct ir_scope *scope, const char *name)
{
    for (; scope != NULL; scope = scope->parent) {
        void *val = NULL;
        if (hashmap_lookup(scope->bindings, name, &val) == 1)
            return (int)(intptr_t)val - 1;
    }

    return -1;
}

static int
ir_scope_insert(struct ir_scope *scope, const char *name, int temp)
{
    // +1 keeps temp 0 distinct from NULL, which hashmap_insert rejects
    return hashmap_insert(scope->bindings, name, (void *)(intptr_t)(temp + 1));
}

static int
translate_expr(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope)
{
    assert(node != NULL && p != NULL && ctx != NULL && scope != NULL);

    switch (node->type) {
        case NODE_NUMBER: {
            int dst = ir_new_temp(p);
            if (ir_emit(p, IR_OP_LOAD_IMM, dst, IR_NONE, IR_NONE,
                        ENCODE_INTEGER(node->as.number)) != 0)
                return -1;

            return dst;
        }

        case NODE_STRING: {
            int64_t idx = ir_program_str_push(p, node->as.string);
            if (idx < 0)
                return -1;

            int dst = ir_new_temp(p);
            if (ir_emit(p, IR_OP_LOAD_STR, dst, IR_NONE, IR_NONE, idx) != 0)
                return -1;

            return dst;
        }

        case NODE_SYMBOL: {
            int temp = ir_scope_lookup(scope, node->as.symbol);
            if (temp >= 0)
                return temp;

            // Not let-bound: the self-evaluating globals, or a symbol
            // semantic analysis accepted but translation can't place yet
            if (strcmp(node->as.symbol, "nil") == 0)
                return translate_nil(p);

            if (strcmp(node->as.symbol, "t") == 0) {
                int dst = ir_new_temp(p);
                if (ir_emit(p, IR_OP_LOAD_IMM, dst, IR_NONE, IR_NONE, T_WORD) != 0)
                    return -1;

                return dst;
            }

            error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                    "IR translation for symbol '%s' not yet implemented", node->as.symbol);
            return -1;
        }

        case NODE_LIST: {
            if (node->as.list.count == 0)
                return translate_nil(p); // () is nil

            struct ast_node *op = node->as.list.children[0];

            // Semantic analysis rejects non-symbol heads; by the time
            // translation runs this is a pipeline invariant
            assert(op->type == NODE_SYMBOL);

            enum builtin_type kind = builtin_kind(op->as.symbol, strlen(op->as.symbol));

            switch (kind) {
                case BUILTIN_PLUS:
                case BUILTIN_MINUS:
                case BUILTIN_MUL:
                case BUILTIN_DIV:
                    return translate_arithmetic(node, p, ctx, scope, kind);

                case BUILTIN_LET:
                    return translate_let(node, p, ctx, scope);

                case BUILTIN_IF:
                    return translate_if(node, p, ctx, scope);

                default:
                    error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                            "IR translation for '%s' not yet implemented", op->as.symbol);
                    return -1;
            }
        }

        case NODE_NIL:
            return translate_nil(p);
    }

    return -1;
}

/*
 * Arithmetic forms fold left, matching Common Lisp:
 *   (- a b c)  ==  (a - b) - c
 *
 * Unary forms follow Common Lisp too:
 *   (+ x) == x    (* x) == x    (- x) == -x    (/ x) == 1/x
 */
static int
translate_arithmetic(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope, enum builtin_type kind)
{
    enum ir_ops op;

    switch (kind) {
        case BUILTIN_PLUS:  op = IR_OP_ADD; break;
        case BUILTIN_MINUS: op = IR_OP_SUB; break;
        case BUILTIN_MUL:   op = IR_OP_MUL; break;
        case BUILTIN_DIV:   op = IR_OP_DIV; break;
        default:
            assert(0 && "translate_arithmetic called with non-arithmetic builtin");
            return -1; // unreachable
    }

    size_t nargs = node->as.list.count - 1;
    assert(nargs >= 1); // semantic analysis rejects zero-argument arithmetic

    int acc = translate_expr(node->as.list.children[1], p, ctx, scope);
    if (acc < 0)
        return -1;

    if (nargs == 1) {
        switch (kind) {
            case BUILTIN_PLUS:
            case BUILTIN_MUL:
                return acc; // identity

            case BUILTIN_MINUS: {
                int dst = ir_new_temp(p);
                if (ir_emit(p, IR_OP_NEG, dst, acc, IR_NONE, 0) != 0)
                    return -1;

                return dst;
            }

            case BUILTIN_DIV: {
                int one = ir_new_temp(p);
                if (ir_emit(p, IR_OP_LOAD_IMM, one, IR_NONE, IR_NONE, ENCODE_INTEGER(1)) != 0)
                    return -1;

                int dst = ir_new_temp(p);
                if (ir_emit(p, IR_OP_DIV, dst, one, acc, 0) != 0)
                    return -1;

                return dst;
            }

            default:
                assert(0 && "unreachable: kind narrowed to arithmetic above");
                return -1;
        }
    }

    for (size_t i = 2; i < node->as.list.count; ++i) {
        int rhs = translate_expr(node->as.list.children[i], p, ctx, scope);
        if (rhs < 0)
            return -1;

        int dst = ir_new_temp(p);
        if (ir_emit(p, op, dst, acc, rhs, 0) != 0)
            return -1;

        acc = dst;
    }

    return acc;
}

// nil is an immediate constant, so loading it is just a LOAD_IMM
static int
translate_nil(struct ir_program *p)
{
    int dst = ir_new_temp(p);
    if (ir_emit(p, IR_OP_LOAD_IMM, dst, IR_NONE, IR_NONE, NIL_WORD) != 0)
        return -1;

    return dst;
}

/*
 * (if test then [else])
 *
 * Lisp truthiness: only nil is false. A one-armed if whose test fails
 * evaluates to nil.
 *
 * Both arms copy their value into a shared result temp -- TAC's
 * stand-in for an SSA phi node -- so `res` is written twice: single
 * assignment ends at join points.
 *
 *   t_test = <test>
 *   jmp_if_nil t_test, L_else
 *   <then>; res = t_then
 *   jmp L_end
 * L_else:
 *   <else>; res = t_else
 * L_end:
 */
static int
translate_if(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope)
{
    // analyze_if validated 2 or 3 arguments; pipeline invariant
    assert(node->as.list.count == 3 || node->as.list.count == 4);

    int test = translate_expr(node->as.list.children[1], p, ctx, scope);
    if (test < 0)
        return -1;

    int l_else = ir_new_label(p);
    int l_end = ir_new_label(p);
    int res = ir_new_temp(p);

    if (ir_emit(p, IR_OP_JMP_IF_NIL, IR_NONE, test, IR_NONE, l_else) != 0)
        return -1;

    int then = translate_expr(node->as.list.children[2], p, ctx, scope);
    if (then < 0)
        return -1;

    if (ir_emit(p, IR_OP_MOV, res, then, IR_NONE, 0) != 0 ||
        ir_emit(p, IR_OP_JMP, IR_NONE, IR_NONE, IR_NONE, l_end) != 0 ||
        ir_emit(p, IR_OP_LABEL, IR_NONE, IR_NONE, IR_NONE, l_else) != 0)
        return -1;

    int alt = node->as.list.count == 4
        ? translate_expr(node->as.list.children[3], p, ctx, scope)
        : translate_nil(p);
    if (alt < 0)
        return -1;

    if (ir_emit(p, IR_OP_MOV, res, alt, IR_NONE, 0) != 0 ||
        ir_emit(p, IR_OP_LABEL, IR_NONE, IR_NONE, IR_NONE, l_end) != 0)
        return -1;

    return res;
}

/*
 * (let ((a init-a) (b init-b) ...) body...)
 *
 * Init expressions are translated in the OUTER scope -- this is let,
 * not let* -- matching analyze_let. Each name is then bound to the
 * temp holding its init value; the body's last form is the result.
 */
static int
translate_let(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, struct ir_scope *scope)
{
    // analyze_let validated the ((sym val) ...) shape; pipeline invariant
    struct ast_node *bindings = node->as.list.children[1];
    assert(bindings->type == NODE_LIST);

    struct ir_scope *local = ir_scope_new(scope);
    if (local == NULL)
        return -1;

    for (size_t i = 0; i < bindings->as.list.count; ++i) {
        struct ast_node *binding = bindings->as.list.children[i];
        assert(binding->type == NODE_LIST && binding->as.list.count == 2);

        struct ast_node *var = binding->as.list.children[0];
        struct ast_node *val = binding->as.list.children[1];
        assert(var->type == NODE_SYMBOL);

        int temp = translate_expr(val, p, ctx, scope);
        if (temp < 0 || ir_scope_insert(local, var->as.symbol, temp) != 0) {
            ir_scope_free(local);
            return -1;
        }
    }

    // An empty body evaluates to nil
    int result = node->as.list.count == 2 ? translate_nil(p) : -1;
    for (size_t i = 2; i < node->as.list.count; ++i) {
        result = translate_expr(node->as.list.children[i], p, ctx, local);
        if (result < 0) {
            ir_scope_free(local);
            return -1;
        }
    }

    ir_scope_free(local);
    return result;
}

// The root node produced by parse_program is a list of top-level forms.
// Translate each child on its own and return the last form's value to the runtime.
int
translate_program(struct ast_node *program, struct ir_program *p, struct error_ctx *ctx)
{
    assert(program != NULL && p != NULL && ctx != NULL);

    struct ir_scope *global = ir_scope_new(NULL);
    if (global == NULL)
        return -1;

    int last = IR_NONE;

    if (program->type == NODE_LIST) {
        for (size_t i = 0; i < program->as.list.count; ++i) {
            last = translate_expr(program->as.list.children[i], p, ctx, global);
            if (last < 0) {
                ir_scope_free(global);
                return -1;
            }
        }
    } else {
        last = translate_expr(program, p, ctx, global);
        if (last < 0) {
            ir_scope_free(global);
            return -1;
        }
    }

    ir_scope_free(global);

    if (last != IR_NONE)
        if (ir_emit(p, IR_OP_RETURN, IR_NONE, last, IR_NONE, 0) != 0)
            return -1;

    return 0;
}

static const char *ir_op_names[IR_OP_COUNT] = {
    [IR_OP_NOP]          = "nop",
    [IR_OP_LOAD_IMM]     = "load_imm",
    [IR_OP_LOAD_STR]     = "load_str",
    [IR_OP_MOV]          = "mov",
    [IR_OP_ADD]          = "add",
    [IR_OP_SUB]          = "sub",
    [IR_OP_MUL]          = "mul",
    [IR_OP_DIV]          = "div",
    [IR_OP_NEG]          = "neg",
    [IR_OP_CALL_BUILTIN] = "call_builtin",
    [IR_OP_CALL]         = "call",
    [IR_OP_RETURN]       = "ret",
    [IR_OP_JMP]          = "jmp",
    [IR_OP_JMP_IF_NIL]   = "jmp_if_nil",
    [IR_OP_LABEL]        = "label",
};

void
ir_program_print(const struct ir_program *p, FILE *out)
{
    if (p == NULL || out == NULL)
        return;

    fprintf(out, "; %zu instructions, %d temps, %zu strings\n",
            p->count, p->temp_count, p->str_count);

    for (size_t i = 0; i < p->count; ++i) {
        const struct ir_instr *instr = &p->instructions[i];

        switch (instr->op) {
            case IR_OP_LOAD_IMM:
                if (IS_NIL(instr->imm))
                    fprintf(out, "t%d = nil\n", instr->dst);
                else if (IS_T(instr->imm))
                    fprintf(out, "t%d = t\n", instr->dst);
                else
                    fprintf(out, "t%d = %ld\n", instr->dst, DECODE_INTEGER(instr->imm));
                break;

            case IR_OP_LOAD_STR:
                fprintf(out, "t%d = \"%s\"\t; str[%ld]\n",
                        instr->dst, p->strings[instr->imm], (long)instr->imm);
                break;

            case IR_OP_ADD:
            case IR_OP_SUB:
            case IR_OP_MUL:
            case IR_OP_DIV:
                fprintf(out, "t%d = %s t%d, t%d\n",
                        instr->dst, ir_op_names[instr->op], instr->src1, instr->src2);
                break;

            case IR_OP_NEG:
                fprintf(out, "t%d = neg t%d\n", instr->dst, instr->src1);
                break;

            case IR_OP_MOV:
                fprintf(out, "t%d = t%d\n", instr->dst, instr->src1);
                break;

            case IR_OP_JMP:
                fprintf(out, "jmp L%ld\n", (long)instr->imm);
                break;

            case IR_OP_JMP_IF_NIL:
                fprintf(out, "jmp_if_nil t%d, L%ld\n", instr->src1, (long)instr->imm);
                break;

            case IR_OP_LABEL:
                fprintf(out, "L%ld:\n", (long)instr->imm);
                break;

            case IR_OP_RETURN:
                fprintf(out, "ret t%d\n", instr->src1);
                break;

            default:
                fprintf(out, "%s dst=%d src1=%d src2=%d imm=%ld\n",
                        ir_op_names[instr->op], instr->dst, instr->src1,
                        instr->src2, (long)instr->imm);
                break;
        }
    }
}
