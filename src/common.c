#include "common.h"

#ifndef _GNU_SOURCE

char *strdup(const char *s)
{
    const size_t len = strlen(s);
    char *dup        = malloc(len + 1);
    if (!dup) return NULL;

    return memcpy(dup, s, len + 1);
}

#endif // _GNU_SOURCE
