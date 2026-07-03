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

    File: mlispc_strndup.c
    Purpose: Implements a portable strndup replacement.
*/

#include <stdlib.h>
#include <string.h>

#include "mlispc_strndup.h"

char *
mlispc_strndup(const char *s, size_t n)
{
    char *p = malloc(n + 1);

    if (!p) return NULL;

    memcpy(p, s, n);
    p[n] = '\0';

    return p;
}
