//
// Created by escha on 28.03.26.
//

#ifndef PARSER_H_
#define PARSER_H_

#include <stddef.h>

#define OP_LIST\
    X(OP_AND) \
    X(OP_OR) \
    X(OP_NOT) \
    X(OP_TAG) \
    X(OP_TAGGED) \
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

typedef struct Lexer Lexer;

Lexer* lexer_new(const char *source_code);
void lexer_free(Lexer *lexer);
void parse_expr(Lexer *lexer, Ops *ops);
char* op_to_str(Op op);

#endif // PARSER_H_
