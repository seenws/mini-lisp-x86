#ifndef MINI_LISP_X86_SRC_COMPILER_IR_H_
#define MINI_LISP_X86_SRC_COMPILER_IR_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "ast.h"
#include "../util/error.h"

/*
 * The IR is three-address code (TAC)
 *
 *   t2 = add t0, t1      -->  { op=IR_OP_ADD, dst=2, src1=0, src2=1 }
 *
 * Temps (t0, t1, ...) are virtual registers: unlimited in number,
 * numbered by a per-program counter. Mapping temps onto machine
 * registers and stack slots is codegen's job, not the IR's. A field
 * that an instruction does not use holds IR_NONE.
 *
 * Every temp holds a tagged word (see the encoding macros below).
 * Consequences for codegen:
 *   ADD/SUB  work directly on tagged integers (the 00 tag survives)
 *   MUL      (a<<2) * (b<<2) == (a*b)<<4, so untag one operand first
 *   DIV      untag both operands, divide, re-encode the quotient
 *   truth    only nil is false: JMP_IF_NIL is `cmp reg, NIL_WORD; je`
 *
 * Control flow stays a flat instruction stream: IR_OP_LABEL is a
 * no-op marker carrying a label id (from a per-program counter), and
 * jumps name label ids in imm. Codegen turns label ids into .L<n>:.
 */

#define IR_NONE (-1)  // "this operand field is unused"

typedef int64_t word;  // 64-bit tagged object

/*
 * INTEGER_TAG = 0b00
 * INTEGER_MASK = 0b11
 * INTEGER_SHIFT = 2
 *
 * Example integer layout:
 *   value = 42
 *   raw bits:        0b000...00101010
 *   encoded integer: 0b000...00101010 << 2
 *                  = 0b000...10101000
 */
#define INTEGER_TAG   0x0
#define INTEGER_MASK  0x3
#define INTEGER_SHIFT 2

/*
 * STRING_TAG = 0b001
 *
 * Pointer alignment guarantees low 3 bits are 0:
 *   ptr = 0x1000 = 0b...000100000000000
 *   tagged      = ptr | 0b001
 *               = 0b...000100000000001
 */
#define STRING_TAG    0x1

/*
 * nil and t are immediate constants under the low-3-bits tag 0b111,
 * which cannot collide with integers (low 2 bits 00) or strings
 * (low 3 bits 001). Distinguished from each other by bit 3.
 *
 *   nil = 0b0111 (0x7)   the only false value; also the empty list
 *   t   = 0b1111 (0xF)   the canonical true value
 */
#define NIL_WORD ((word)0x7)
#define T_WORD   ((word)0xF)

#define IS_NIL(obj) ((word)(obj) == NIL_WORD)
#define IS_T(obj)   ((word)(obj) == T_WORD)

/*
 * ENCODE_INTEGER(value)
 *
 * Behavior:
 *   Shifts the integer left by INTEGER_SHIFT bits,
 *   placing the INTEGER_TAG (00) in the low bits.
 *
 * Example:
 *   value = 5
 *   binary value:    0b000...0101
 *   shift left 2:    0b000...010100
 *   tag bits:                        00
 *   result:         0b000...010100  (encoded integer)
 */
#define ENCODE_INTEGER(value) \
    ((word)(value) << INTEGER_SHIFT)

/*
 * DECODE_INTEGER(obj)
 *
 * Behavior:
 *   Shifts the tagged integer right by INTEGER_SHIFT bits,
 *   discarding the tag and restoring the original value.
 *
 * Example:
 *   obj = 0b000...010100
 *   shift right 2 → 0b000...0101
 *   result = 5
 *
 * Note:
 *   Arithmetic shift preserves sign for negative integers.
 */
#define DECODE_INTEGER(obj) \
    ((long)((word)(obj) >> INTEGER_SHIFT))

/*
 * ENCODE_STRING(ptr)
 *
 * Behavior:
 *   Sets the STRING_TAG bit in the low bits of a pointer.
 *
 * Example:
 *   ptr = 0x1000
 *       = 0b...000100000000000
 *   STRING_TAG = 0b001
 *
 *   ptr | STRING_TAG
 * = 0b...000100000000001
 *
 * Result:
 *   Tagged string object
 */
#define ENCODE_STRING(ptr) \
    ((word)(ptr) | STRING_TAG)

/*
 * DECODE_STRING(obj)
 *
 * Behavior:
 *   Clears the STRING_TAG bit to recover the original pointer.
 *
 * Example:
 *   obj = 0b...000100000000001
 *   ~STRING_TAG = 0b...111111111110
 *
 *   obj & ~STRING_TAG
 * = 0b...000100000000000
 * = original pointer (0x1000)
 */
#define DECODE_STRING(obj) \
    ((word)(obj) & ~(word)STRING_TAG)

/*
 * IS_INTEGER(obj)
 *
 * Behavior:
 *   Extracts the low 2 bits and compares them to INTEGER_TAG.
 *
 * Example (integer):
 *   obj = 0b...010100
 *   obj & 0b11 = 0b00 → INTEGER_TAG → true
 *
 * Example (string):
 *   obj = 0b...000001
 *   obj & 0b11 = 0b01 ≠ INTEGER_TAG → false
 */
#define IS_INTEGER(obj) \
    (((word)(obj) & INTEGER_MASK) == INTEGER_TAG)

/*
 * IS_STRING(obj)
 *
 * Behavior:
 *   Extracts the low 3 bits and compares them to STRING_TAG.
 *
 * Example (string):
 *   obj = 0b...000001
 *   obj & 0b111 = 0b001 → STRING_TAG → true
 *
 * Example (integer):
 *   obj = 0b...010100
 *   obj & 0b111 = 0b100 ≠ STRING_TAG → false
 */
#define IS_STRING(obj) \
    (((word)(obj) & 0x7) == STRING_TAG)

enum ir_ops {
    IR_OP_NOP = 0,
    IR_OP_LOAD_IMM,     // dst = imm (tagged constant: integer, nil or t)
    IR_OP_LOAD_STR,     // dst = tagged address of strings[imm]
    IR_OP_MOV,          // dst = src1 (join-point copy; ends single assignment)
    IR_OP_ADD,          // dst = src1 + src2
    IR_OP_SUB,          // dst = src1 - src2
    IR_OP_MUL,          // dst = src1 * src2
    IR_OP_DIV,          // dst = src1 / src2
    IR_OP_NEG,          // dst = -src1
    IR_OP_CALL_BUILTIN,
    IR_OP_CALL,
    IR_OP_RETURN,       // return src1 to the runtime
    IR_OP_JMP,          // jump to label imm
    IR_OP_JMP_IF_NIL,   // jump to label imm when src1 == NIL_WORD
    IR_OP_LABEL,        // no-op jump target; imm = label id
    IR_OP_COUNT
};

struct ir_instr {
    enum ir_ops op;
    int dst;            // destination temp, IR_NONE if unused
    int src1;           // source temps, IR_NONE if unused
    int src2;
    int64_t imm;        // immediate payload (LOAD_IMM value, LOAD_STR index)
};

struct ir_program {
    struct ir_instr *instructions;
    size_t count;
    size_t capacity;

    char **strings; // .data section
    size_t str_count;
    size_t str_capacity;

    int temp_count;  // next virtual register number
    int label_count; // next label id
};

struct ir_program  *ir_program_new      (void);
void                ir_program_free     (struct ir_program *p);
int                 ir_emit             (struct ir_program *p, enum ir_ops op,
                                         int dst, int src1, int src2, int64_t imm);
int64_t             ir_program_str_push (struct ir_program *p, const char *str);
int                 ir_new_temp         (struct ir_program *p);
int                 ir_new_label        (struct ir_program *p);
void                ir_program_print    (const struct ir_program *p, FILE *out);

/* Translation must only run on a semantically validated AST */

/* Translate a whole program (root list of top-level forms) and emit a
 * final RETURN of the last form's value. Returns 0 on success, -1 on error. */
int translate_program(struct ast_node *program, struct ir_program *p, struct error_ctx *ctx);

#endif
