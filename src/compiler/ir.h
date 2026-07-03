#ifndef MINI_LISP_X86_SRC_COMPILER_IR_H_
#define MINI_LISP_X86_SRC_COMPILER_IR_H_

#include <stdint.h>
#include <stddef.h>

#include "ast.h"

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
    IR_OP_LOAD_IMM,
    IR_OP_LOAD_STR,
    IR_OP_LOAD_SYM,
    IR_OP_LOAD_NIL,
    IR_OP_PUSH,
    IR_OP_POP,
    IR_OP_ADD,
    IR_OP_SUB,
    IR_OP_DIV,
    IR_OP_MUL,
    IR_OP_CALL_BUILTIN,
    IR_OP_CALL,
    IR_OP_RETURN,
    IR_OP_JMP,
    IR_OP_COUNT
};

struct ir_instr {
    enum ir_ops op;
    int64_t operand;
};

struct ir_program {
    struct ir_instr *instructions;
    size_t count;
    size_t capacity;

    char **strings; // .data section
    size_t str_count;
    size_t str_capacity;
};

struct ir_program  *ir_program_new      (void);
void                ir_program_free     (struct ir_program *p);
int                 ir_program_push     (struct ir_program *p, enum ir_ops op, int64_t operand);
size_t              ir_program_str_push (struct ir_program *p, const char *str);

int translate_code_block(struct ast_node *node, struct ir_program *p);

#endif
