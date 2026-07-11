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

    File: codegen.c
    Purpose: Lowers the TAC IR to x86_64 assembly (GNU as, Intel syntax)
    with spill-everything register allocation.
*/

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "codegen.h"
#include "ir.h"

/*
 * Tagged-word lowering (see ir.h for the encoding):
 *   ADD/SUB  tag-transparent: (a<<2) +- (b<<2) == (a+-b)<<2
 *   NEG      tag-transparent for the same reason
 *   MUL      (a<<2)*(b<<2) == (a*b)<<4, so untag one operand first
 *   DIV      untag both, idiv, re-tag the quotient
 *   truth    cmp against NIL_WORD; only nil is false
 *
 * String literals live in .data under .LC<n> labels, .align 8 so the
 * low three bits of their address are free for the string tag.
 */

/* Stack slot of temp t. Slots start below the saved rbp, so t0 is at
 * [rbp-8], t1 at [rbp-16], ... */
static long
slot(int temp)
{
    assert(temp >= 0);

    return -8L * (temp + 1);
}

static int
fits_imm32(int64_t v)
{
    return v >= INT32_MIN && v <= INT32_MAX;
}

/* Emit a gas string literal producing exactly the bytes of `s`: the
 * lexer stores string contents raw, so every byte must round-trip. */
static void
emit_string_literal(FILE *out, const char *s)
{
    fputc('"', out);

    for (const unsigned char *c = (const unsigned char *)s; *c != '\0'; ++c) {
        if (*c == '"' || *c == '\\')
            fprintf(out, "\\%c", *c);
        else if (*c < 0x20 || *c == 0x7F)
            fprintf(out, "\\%03o", *c);
        else
            fputc(*c, out);
    }

    fputc('"', out);
}

static void
emit_data_section(const struct ir_program *p, FILE *out)
{
    if (p->str_count == 0)
        return;

    fputs("    .data\n", out);

    for (size_t i = 0; i < p->str_count; ++i) {
        fputs("    .align 8\n", out);
        fprintf(out, ".LC%zu:\n", i);
        fputs("    .string ", out);
        emit_string_literal(out, p->strings[i]);
        fputc('\n', out);
    }

    fputc('\n', out);
}

/* mov rax, <imm> for any 64-bit value: gas only accepts imm64 through
 * movabs. */
static void
emit_load_rax_imm(FILE *out, int64_t imm)
{
    if (fits_imm32(imm))
        fprintf(out, "    mov rax, %ld\n", (long)imm);
    else
        fprintf(out, "    movabs rax, %ld\n", (long)imm);
}

static void
emit_return(FILE *out, int src)
{
    fprintf(out, "    mov rax, [rbp%ld]\n", slot(src));
    fputs("    leave\n", out);
    fputs("    ret\n", out);
}

static void
emit_instr(const struct ir_instr *instr, FILE *out)
{
    switch (instr->op) {
        case IR_OP_NOP:
            break;

        case IR_OP_LOAD_IMM:
            fprintf(out, "    # t%d = load_imm %ld\n", instr->dst, (long)instr->imm);
            if (fits_imm32(instr->imm)) {
                fprintf(out, "    mov qword ptr [rbp%ld], %ld\n",
                        slot(instr->dst), (long)instr->imm);
            } else {
                emit_load_rax_imm(out, instr->imm);
                fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            }
            break;

        case IR_OP_LOAD_STR:
            fprintf(out, "    # t%d = load_str str[%ld]\n", instr->dst, (long)instr->imm);
            /* rip-relative so the code links as PIE; or sets the tag */
            fprintf(out, "    lea rax, .LC%ld[rip]\n", (long)instr->imm);
            fprintf(out, "    or rax, %d\n", STRING_TAG);
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_MOV:
            fprintf(out, "    # t%d = t%d\n", instr->dst, instr->src1);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_ADD:
            fprintf(out, "    # t%d = add t%d, t%d\n", instr->dst, instr->src1, instr->src2);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fprintf(out, "    add rax, [rbp%ld]\n", slot(instr->src2));
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_SUB:
            fprintf(out, "    # t%d = sub t%d, t%d\n", instr->dst, instr->src1, instr->src2);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fprintf(out, "    sub rax, [rbp%ld]\n", slot(instr->src2));
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_MUL:
            fprintf(out, "    # t%d = mul t%d, t%d\n", instr->dst, instr->src1, instr->src2);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fprintf(out, "    sar rax, %d\n", INTEGER_SHIFT);
            fprintf(out, "    imul rax, qword ptr [rbp%ld]\n", slot(instr->src2));
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_DIV:
            fprintf(out, "    # t%d = div t%d, t%d\n", instr->dst, instr->src1, instr->src2);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fprintf(out, "    sar rax, %d\n", INTEGER_SHIFT);
            fprintf(out, "    mov rcx, [rbp%ld]\n", slot(instr->src2));
            fprintf(out, "    sar rcx, %d\n", INTEGER_SHIFT);
            fputs("    cqo\n", out); /* sign-extend rax into rdx:rax */
            fputs("    idiv rcx\n", out);
            fprintf(out, "    shl rax, %d\n", INTEGER_SHIFT);
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_NEG:
            fprintf(out, "    # t%d = neg t%d\n", instr->dst, instr->src1);
            fprintf(out, "    mov rax, [rbp%ld]\n", slot(instr->src1));
            fputs("    neg rax\n", out);
            fprintf(out, "    mov [rbp%ld], rax\n", slot(instr->dst));
            break;

        case IR_OP_JMP:
            fprintf(out, "    jmp .L%ld\n", (long)instr->imm);
            break;

        case IR_OP_JMP_IF_NIL:
            fprintf(out, "    # jmp_if_nil t%d, L%ld\n", instr->src1, (long)instr->imm);
            fprintf(out, "    cmp qword ptr [rbp%ld], %ld\n",
                    slot(instr->src1), (long)NIL_WORD);
            fprintf(out, "    je .L%ld\n", (long)instr->imm);
            break;

        case IR_OP_LABEL:
            fprintf(out, ".L%ld:\n", (long)instr->imm);
            break;

        case IR_OP_RETURN:
            fprintf(out, "    # ret t%d\n", instr->src1);
            emit_return(out, instr->src1);
            break;

        case IR_OP_CALL_BUILTIN:
        case IR_OP_CALL:
            assert(0 && "codegen: CALL ops not lowered yet (translator never emits them)");
            break;

        default:
            assert(0 && "codegen: unknown IR op");
            break;
    }
}

int
codegen_emit(const struct ir_program *p, FILE *out)
{
    if (p == NULL || out == NULL)
        return -1;

    /* One slot per temp; keep rsp 16-aligned (it is on function entry
     * after `push rbp`, and future CALL lowering will rely on it). */
    long frame = (8L * p->temp_count + 15L) & ~15L;

    fputs("    .intel_syntax noprefix\n\n", out);

    emit_data_section(p, out);

    fputs("    .text\n", out);
    fputs("    .globl lisp_entry\n", out);
    fputs("lisp_entry:\n", out);
    fputs("    push rbp\n", out);
    fputs("    mov rbp, rsp\n", out);
    if (frame > 0)
        fprintf(out, "    sub rsp, %ld\n", frame);

    for (size_t i = 0; i < p->count; ++i)
        emit_instr(&p->instructions[i], out);

    /* An empty program has no RETURN; fall back to nil so lisp_entry
     * never returns garbage. */
    if (p->count == 0 || p->instructions[p->count - 1].op != IR_OP_RETURN) {
        fprintf(out, "    mov rax, %ld    # implicit nil\n", (long)NIL_WORD);
        fputs("    leave\n", out);
        fputs("    ret\n", out);
    }

    /* Non-executable stack marker; without it the linker warns and
     * marks the whole process stack executable. */
    fputs("\n    .section .note.GNU-stack,\"\",@progbits\n", out);

    return ferror(out) ? -1 : 0;
}
