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
