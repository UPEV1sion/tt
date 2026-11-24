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

#endif // COMMON_H_
