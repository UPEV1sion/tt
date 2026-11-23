//
// Created by escha on 28.03.26.
//

#ifndef COMMON_H_
#define COMMON_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define assertmsg(cond, ...)              \
    do                                    \
    {                                     \
        if (!(cond))                      \
        {                                 \
            fprintf(stderr, __VA_ARGS__); \
            exit(1);                      \
        }                                 \
    } while (0)

#define BUFSIZE 1024

#ifndef _GNU_SOURCE

static_assert(sizeof(size_t) == sizeof(int64_t), "ERROR: 32 bit env detected, please compile this on a 64 bit machine");

typedef int64_t ssize_t;

char *strdup(const char *s);

#endif // _GNU_SOURCE

#endif // COMMON_H_
