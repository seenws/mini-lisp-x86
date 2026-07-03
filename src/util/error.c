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

    File: error.c
    Purpose: Implements the error context used to accumulate and report
    diagnostics produced during compilation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "error.h"
#include "mlispc_strdup.h"

// initializes a new error context with values
// assumes NULL check beforehand
struct error_ctx *
error_ctx_new(size_t initial_size)
{
    struct error_ctx *ctx = malloc(sizeof(struct error_ctx));
    if (ctx == NULL)
        return NULL;

    if (initial_size < 32)
        initial_size = 32;

    ctx->count = 0;
    ctx->capacity = initial_size;
    ctx->errors = malloc(ctx->capacity * sizeof(struct error));

    if (ctx->errors == NULL) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

// frees the error context and its messages
void
error_ctx_free(struct error_ctx *ctx)
{
    if (ctx == NULL)
        return;

    for (size_t i = 0; i < ctx->count; ++i)
        free((void *)ctx->errors[i].msg);

    free(ctx->errors);
    free(ctx);
}

// doubles the call stack capacity
// returns 0 on success
// returns -1 on failure (e.g., allocation failure)
int
error_ctx_resize(struct error_ctx *ctx)
{
    if (ctx == NULL)
        return -1;

    size_t new_capacity = ctx->capacity * 2;
    if (new_capacity < ctx->capacity) // overflow check
        return 0;

    ctx->errors = realloc(ctx->errors, new_capacity * sizeof(struct error));
    if (ctx->errors == NULL) {
        fprintf(stderr, "Fatal: Unable to `realloc` in error_ctx_resize\n");
        return -1;
    }

    ctx->capacity = new_capacity;
    
    return 0;
}

// Adds an error to the context
// returns 0 on success
// returns -1 on failure
int
error_ctx_push(struct error_ctx *ctx, enum error_severity severity, size_t line, size_t col, const char *fmt, ...)
{
    if (ctx == NULL || fmt == NULL)
        return -1;

    if (ctx->count >= ctx->capacity * 0.75) {
        if (error_ctx_resize(ctx) != 0)
            return -1; // resize failed, abort insert
    }

    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsnprintf(buffer, 1024, fmt, args);
    va_end(args);

    struct error *err = &ctx->errors[ctx->count++];

    err->msg = mlispc_strdup(buffer);
    if (!err->msg) {
        ctx->count--;
        return -1;
    }

    err->severity = severity;
    err->line = line;
    err->col = col;

    return 0;
}

struct error *
error_ctx_pop(struct error_ctx *ctx)
{
    if (ctx == NULL || ctx->count == 0)
        return NULL;

    return &ctx->errors[--ctx->count];
}

static const char *
severity_str(enum error_severity severity)
{
    switch (severity) {
        case ERR_WARNING: return "warning";
        case ERR_ERROR:   return "error";
        case ERR_FATAL:   return "fatal";
        default:          return "unknown";
    }
}

void
error_ctx_print(struct error_ctx *ctx)
{
    if (ctx->count == 0)
        return;

    for (size_t i = 0; i < ctx->count; ++i) {
        struct error *err = &ctx->errors[i];

        if (err->line > 0)
            fprintf(stderr, "%zu:%zu: %s: %s\n", err->line, err->col, severity_str(err->severity), err->msg);
        else
            fprintf(stderr, "%s: %s\n", severity_str(err->severity), err->msg);
    }
}
