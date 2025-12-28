#include <stdio.h>
#include <stdlib.h>

#include "./compiler/lexer.h"
#include "./compiler/token_stream.h"

#define BUFSZ 4096

void
usage(char *progname)
{
    printf("Error: Insufficient arguments provided | Usage: %s <file.lisp>\n", progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    if (argc != 2)
        usage(argv[0]);

    FILE *fp= fopen(argv[1], "r");
    if (fp == NULL) {
        printf("Encountered I/O Error: Unable to read %s\n", argv[1]);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *source = malloc(len + 1);
    fread(source, 1, len, fp);
    source[len] = '\0';
    fclose(fp);

    // Tokenize the whole thing
    struct token_array tokens;
    token_array_init(&tokens);

    struct lexer l = lexer_create(source, len);
    init_token_handlers();

    struct token t;
    while ((t = lexer_next(&l)).type != TOK_END) {
        if (t.type != TOK_COMMENT) {  // skip comments entirely
            token_array_append(&tokens, t);
        }
    }

    // Now create the stream and parse!
    struct token_stream ts = token_stream_create(&tokens);

    // Your parser will go here (e.g., parse_program(&ts))

    // Cleanup
    free(source);
    token_array_free(&tokens);

    return 0;
}
