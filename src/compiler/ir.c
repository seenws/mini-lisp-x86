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
    Purpose: Implements the intermediate representation (IR) program buffer
    and AST-to-IR translation.
*/

#include <stdio.h>
#include <stdlib.h>

#include "ir.h"
#include "ast.h"
#include "../util/mlispc_strdup.h"

#define PROGRAM_MAX_SIZE 1024 // in KiB

void ir_program_free(struct ir_program *p);

struct ir_program *
ir_program_new(void) {
    struct ir_program *p = malloc(sizeof(struct ir_program));
    if (p == NULL)
        return NULL;

    p->instructions = malloc(PROGRAM_MAX_SIZE * sizeof(struct ir_instr));
    p->count = 0;
    p->capacity = PROGRAM_MAX_SIZE;

    p->strings = malloc(PROGRAM_MAX_SIZE * sizeof(char *));
    p->str_count = 0;
    p->str_capacity = PROGRAM_MAX_SIZE;

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

    p->instructions = realloc(p->instructions, new_capacity * sizeof(struct ir_instr));
    if (p->instructions == NULL) {
        fprintf(stderr, "Fatal: Unable to `realloc` in ir_prograam_instr_resize\n");
        return -1;
    }

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

    p->strings = realloc(p->strings, new_str_capacity * sizeof(char *));
    if (p->strings == NULL) {
        fprintf(stderr, "Fatal: Unable to `realloc` in ir_program_str_resize\n");
        return -1;
    }

    p->str_capacity = new_str_capacity;

    return 0;
}

int
ir_program_push(struct ir_program *p, enum ir_ops op, int64_t operand)
{
    if (p->count >= p->capacity * 0.75)
        if (ir_program_instr_resize(p) != 0)
            return -1; // resize failed, abort insert

    p->instructions[p->count].op = op;
    p->instructions[p->count].operand = operand;
    p->count++;

    return 0;
}

size_t
ir_program_str_push(struct ir_program *p, const char *str)
{
    if (p->str_count >= p->str_capacity * 0.75)
        if (ir_program_str_resize(p) != 0)
            return -1; // resize failed, abort insert
    
    p->strings[p->str_count] = mlispc_strdup(str);
    if (p->strings[p->str_count] == NULL)
        return -1;

    return p->str_count++;
}

int
translate_code_block(struct ast_node *node, struct ir_program *p)
{
    if (node == NULL || p == NULL)
        return -1;

    switch (node->type) {
        case NODE_NUMBER: {
                              int64_t tagged = ENCODE_INTEGER(node->as.number);
                              ir_program_push(p, IR_OP_LOAD_IMM, tagged);

                              break;
                          }

        case NODE_STRING: {
                              break;
                          }

        case NODE_SYMBOL: {
                              break;
                          }

        case NODE_LIST:   {
                              break;
                          }

        case NODE_NIL:    {
                              break;
                          }
    }

    return 0;
}
