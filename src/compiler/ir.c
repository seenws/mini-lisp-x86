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
#include "../util/mlispc_strdup.h"

#define IR_INITIAL_CAPACITY 1024 // instructions / strings before first resize

static int translate_arithmetic(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, enum builtin_type kind);

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
translate_expr(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx)
{
    assert(node != NULL && p != NULL && ctx != NULL);

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

        case NODE_SYMBOL:
            error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                    "IR translation for symbol '%s' not yet implemented", node->as.symbol);
            return -1;

        case NODE_LIST: {
            if (node->as.list.count == 0) {
                error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                        "IR translation for nil not yet implemented");
                return -1;
            }

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
                    return translate_arithmetic(node, p, ctx, kind);

                default:
                    error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                            "IR translation for '%s' not yet implemented", op->as.symbol);
                    return -1;
            }
        }

        case NODE_NIL:
            error_ctx_push(ctx, ERR_ERROR, node->line, node->col,
                    "IR translation for nil not yet implemented");
            return -1;
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
translate_arithmetic(struct ast_node *node, struct ir_program *p, struct error_ctx *ctx, enum builtin_type kind)
{
    enum ir_ops op;

    switch (kind) {
        case BUILTIN_PLUS:  op = IR_OP_ADD; break;
        case BUILTIN_MINUS: op = IR_OP_SUB; break;
        case BUILTIN_MUL:   op = IR_OP_MUL; break;
        case BUILTIN_DIV:   op = IR_OP_DIV; break;
        default:
            assert(0 && "translate_arithmetic called with non-arithmetic builtin");
            return -1; // unreachable; keeps NDEBUG builds well-defined
    }

    size_t nargs = node->as.list.count - 1;
    assert(nargs >= 1); // semantic analysis rejects zero-argument arithmetic

    int acc = translate_expr(node->as.list.children[1], p, ctx);
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
        int rhs = translate_expr(node->as.list.children[i], p, ctx);
        if (rhs < 0)
            return -1;

        int dst = ir_new_temp(p);
        if (ir_emit(p, op, dst, acc, rhs, 0) != 0)
            return -1;

        acc = dst;
    }

    return acc;
}

// The root node produced by parse_program is a list of top-level
// forms, not itself a form -- translate each child on its own and
// return the last form's value to the runtime.
int
translate_program(struct ast_node *program, struct ir_program *p, struct error_ctx *ctx)
{
    assert(program != NULL && p != NULL && ctx != NULL);

    int last = IR_NONE;

    if (program->type == NODE_LIST) {
        for (size_t i = 0; i < program->as.list.count; ++i) {
            last = translate_expr(program->as.list.children[i], p, ctx);
            if (last < 0)
                return -1;
        }
    } else {
        last = translate_expr(program, p, ctx);
        if (last < 0)
            return -1;
    }

    if (last != IR_NONE)
        if (ir_emit(p, IR_OP_RETURN, IR_NONE, last, IR_NONE, 0) != 0)
            return -1;

    return 0;
}

static const char *ir_op_names[IR_OP_COUNT] = {
    [IR_OP_NOP]          = "nop",
    [IR_OP_LOAD_IMM]     = "load_imm",
    [IR_OP_LOAD_STR]     = "load_str",
    [IR_OP_LOAD_SYM]     = "load_sym",
    [IR_OP_LOAD_NIL]     = "load_nil",
    [IR_OP_ADD]          = "add",
    [IR_OP_SUB]          = "sub",
    [IR_OP_MUL]          = "mul",
    [IR_OP_DIV]          = "div",
    [IR_OP_NEG]          = "neg",
    [IR_OP_CALL_BUILTIN] = "call_builtin",
    [IR_OP_CALL]         = "call",
    [IR_OP_RETURN]       = "ret",
    [IR_OP_JMP]          = "jmp",
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
