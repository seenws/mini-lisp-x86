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

    File: runtime.c
    Purpose: Process entry for compiled programs; calls the generated
    lisp_entry and prints its tagged-word result.
*/

#include <stdio.h>
#include <stdint.h>

#include "../compiler/ir.h"

extern int64_t lisp_entry(void);

void
runtime_print(int64_t obj)
{
    if (IS_INTEGER(obj))
        printf("%ld", (long)(obj >> INTEGER_SHIFT));
    else if (IS_STRING(obj))
        fputs((const char *)(obj & ~(int64_t)STRING_TAG), stdout);
    else
        printf("#<unknown object type %#lx>", (unsigned long)obj);
}

int
main(void)
{
    int64_t res = lisp_entry();

    runtime_print(res);
    putchar('\n');

    return 0;
}
