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

    File: main.c
    Purpose: Entry point for the mlispc compiler driver; reads a source file
    and drives it through the lexer, parser, and semantic analyzer, reporting
    any diagnostics.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./compiler/lexer.h"
#include "./compiler/token_stream.h"
#include "./compiler/parser.h"
#include "./compiler/semantic.h"
#include "./compiler/ir.h"

#include "./util/error.h"

#define VERSION 1.0
#define DEBUG 1

static inline void
usage(void)
{
    printf("MlispC - A minimal compiler from Common Lisp to x86_64\n");
    puts("usage: mlispc [options] <file.lisp>");
    puts("options:");
    puts("  -v");
    puts("  --version    show version info");
    puts("  -h");
    puts("  --help       show this help");
    exit(1);
}

/* ! returns malloc'd buffer */
char *
read_file(char const *filename)
{
    FILE *fp = fopen(filename, "r");
    long flen;
    char *buffer;

    if (fp == NULL) {
        fprintf(stderr, "minilisp: Cannot open file `%s`.\n", filename);
        perror("Error");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    flen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(flen + 1);

    if (buffer == NULL) {
        fprintf(stderr, "minilisp: Cannot allocate memory.\n");
        perror("Error");
        return NULL;
    }

    fread(buffer, 1, flen, fp);
    buffer[flen] = '\0';
    fclose(fp);

    return buffer;
}

int
main(int argc, char **argv)
{
    if (argc < 2)
        usage();

    char *source = read_file(argv[1]);

    if (source == NULL)
        return 1;

    struct token_array tokens;
    struct token t;

    token_array_init(&tokens);

    struct lexer l = lexer_create(source, strlen(source));
    init_token_handlers();

    while ((t = lexer_next(&l)).type != TOK_END) {
        if (t.type != TOK_COMMENT)
            token_array_append(&tokens, t);
    }

    struct token_stream *ts = token_stream_create(&tokens);

    struct error_ctx *ctx = error_ctx_new(0); // default 32
    int status = 0;

    struct ast_node *program = parse_program(ts, ctx);

    // Each stage runs only if the previous one produced no diagnostics
    if (ctx->count == 0)
        analyze_program(program, ctx);

    if (ctx->count == 0) {
        struct ir_program *ir = ir_program_new();

        if (ir == NULL) {
            fprintf(stderr, "Fatal: Unable to allocate IR program\n");
            status = 1;
        } else {
            if (translate_program(program, ir, ctx) != 0)
                status = 1;
            else if (DEBUG)
                ir_program_print(ir, stdout);

            ir_program_free(ir);
        }
    }

    if (ctx->count != 0) {
        error_ctx_print(ctx);
        status = 1;
    }

    ast_node_free(program);
    free(ts);
    free(source);
    token_array_free(&tokens);
    error_ctx_free(ctx);

    return status;
}
