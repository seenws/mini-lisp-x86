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

    File: codegen.h
    Purpose: Public interface for lowering a translated IR program to
    x86_64 assembly (GNU as, Intel syntax).
*/

#ifndef MINI_LISP_X86_SRC_COMPILER_CODEGEN_H_
#define MINI_LISP_X86_SRC_COMPILER_CODEGEN_H_

#include <stdio.h>

#include "ir.h"

/*
 * Lower `p` to x86_64 assembly on `out`, producing one function,
 * `lisp_entry`, that the runtime calls and whose tagged-word result
 * (in rax) it prints.
 *
 * Register allocation is "spill everything": temp t lives in the
 * stack slot [rbp - 8*(t+1)] for the whole program, and every
 * instruction loads its operands from and stores its result to those
 * slots through rax/rcx. No temp ever stays in a register across
 * instructions, so correctness never depends on liveness analysis.
 * Linear-scan allocation replaces this later.
 *
 * Codegen must only run on a fully translated program; IR ops the
 * translator cannot emit yet are asserted as pipeline invariants.
 *
 * Returns 0 on success, -1 on error.
 */
int codegen_emit(const struct ir_program *p, FILE *out);

#endif
