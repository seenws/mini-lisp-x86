#include <stdlib.h>
#include <string.h>

#include "mlispc_strdup.h"

char *
mlispc_strdup(const char *s)
{
    size_t len = strlen(s);
    void *new = malloc(len);

    if (new == NULL)
        return NULL;

    return (char *)memcpy(new, s, len);
}
