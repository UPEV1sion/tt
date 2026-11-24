//
// Created by escha on 28.03.26.
//

#ifndef FILTER_H_
#define FILTER_H_

#include <stddef.h>

#include "task.h"

#define OP_LIST    \
    X(OP_AND)      \
    X(OP_OR)       \
    X(OP_NOT)      \
    X(OP_TAG)      \
    X(OP_TAGGED)   \
    X(OP_UNTAGGED) \
    X(OP_ID)

typedef enum {
#define X(a) a,
    OP_LIST
#undef X
} OpCode;

typedef struct {
    OpCode code;
    char *lexeme;
} Op;

typedef struct {
    size_t count;
    size_t capacity;
    Op *items;
} Ops;

typedef struct {
    size_t count;
    size_t capacity;
    bool *items;
} Stack;

typedef struct Lexer Lexer;

Lexer *lexer_new(const char *source_code);
void lexer_free(Lexer *lexer);
void parse_expr(Lexer *lexer, Ops *ops);
char *op_to_str(Op op);
void op_free(Ops *ops);

void filter_tasks(Tasks *tasks, Ops *ops);

#endif // FILTER_H_
