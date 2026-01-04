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

// frees the call stack
void
error_ctx_free(struct error_ctx *ctx)
{
    if (ctx == NULL)
        return;

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
error_ctx_push(struct error_ctx *ctx, enum error_severity severity, const char *fmt, ...)
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
    if (!err->msg)
        return -1;

    err->severity = severity;

    printf("%s\n", err->msg);

    return 0;
}

struct error *
error_ctx_pop(struct error_ctx *ctx)
{
    if (ctx == NULL || ctx->count == 0)
        return NULL;

    return &ctx->errors[--ctx->count];
}

void
error_ctx_print(struct error_ctx *ctx)
{
    if (ctx->count == 0)
        return;

    for (size_t i = 0; i < ctx->count; ++i)
        printf("%s\n", ctx->errors[i].msg);
}
