#ifndef MINI_LISP_X86_SRC_UTIL_ERROR_H_
#define MINI_LISP_X86_SRC_UTIL_ERROR_H_

#include <stddef.h>

enum error_severity {
    ERR_WARNING,
    ERR_ERROR,
    ERR_FATAL
};

struct error {
    enum error_severity severity;
    const char *msg;
    size_t line;    // 1-based source position, 0 if unknown
    size_t col;
};

struct error_ctx {
    struct error *errors;
    size_t count;
    size_t capacity;
};

struct error_ctx   *error_ctx_new       (size_t initial_size);
int                 error_ctx_push      (struct error_ctx *ctx, enum error_severity severity, size_t line, size_t col, const char *fmt, ...);
int                 error_ctx_resize    (struct error_ctx *ctx);
struct error       *error_ctx_pop       (struct error_ctx *ctx);
void                error_ctx_free      (struct error_ctx *ctx);
void                error_ctx_print     (struct error_ctx *ctx);

#endif
