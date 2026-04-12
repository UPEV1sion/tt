//
// Created by escha on 28.03.26.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "parser.h"
#include "da.h"
#include "common.h"

#define ARENA_IMPLEMENTATION
#include "arena.h"

typedef enum {
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_TAG,
    TOK_TAGGED,
    TOK_UNTAGGED,
    TOK_ID,
    TOK_EOF,
    TOK_INVALID,
} TokenType;

struct {
    TokenType type;
    const char *lexeme;
} keywords[] = {
    {TOK_OR, "or"},
    {TOK_AND, "and"},
    {TOK_NOT, "not"},
    {TOK_TAGGED, "tagged"},
    {TOK_UNTAGGED, "untagged"},
};

typedef struct {
    TokenType type;
    char *lexeme;
    size_t pos;
} Token;

struct Lexer {
    size_t pos;
    char *input;
    Token cur_tok;
    Token last_tok;
    int cur_char;
    Arena *arena;
};

Token token_new(Arena *arena, const TokenType type, const char *lexeme, const size_t pos)
{
    return (Token) {
        .type = type,
        .lexeme = lexeme ? arena_strdup(arena, lexeme) : NULL,
        .pos = pos,
    };
}

void lexer_advance(Lexer *lexer)
{
    lexer->cur_char = (unsigned char) lexer->input[lexer->pos++];
}

void lexer_trim_left(Lexer *lexer)
{
    while(isspace((unsigned char) lexer->cur_char))
    {
        lexer_advance(lexer);
    }
}

void lexer_match_char(Lexer *lexer, const int c)
{
    assertmsg(lexer->cur_char == c, "ERROR: unexpected char '%c'\n", c);
    lexer_advance(lexer);
}

Token lexer_next_tok(Lexer *lexer)
{
    lexer_trim_left(lexer);

    Token tok = token_new(lexer->arena, TOK_INVALID, NULL, 0);
    switch(lexer->cur_char)
    {
        case '\0': {
            tok = token_new(lexer->arena, TOK_EOF, NULL, lexer->pos);
        } break;
        case '(': {
            tok = token_new(lexer->arena, TOK_LPAREN, NULL, lexer->pos);
            lexer_advance(lexer);
        } break;
        case ')': {
            tok = token_new(lexer->arena, TOK_RPAREN, NULL, lexer->pos);
            lexer_advance(lexer);
        } break;
        default : {
            if(lexer->cur_char == '.')
            {
                const size_t start = lexer->pos;
                lexer_advance(lexer);

                size_t pos = 0;
                char buffer[BUFSIZE];

                while(isalnum((unsigned char) lexer->cur_char) && pos < BUFSIZE - 1)
                {
                    buffer[pos++] = (unsigned char) lexer->cur_char;
                    lexer_advance(lexer);
                }

                buffer[pos] = 0;
                tok = token_new(lexer->arena, TOK_TAG, buffer, start);
            }
            else if(isalpha(lexer->cur_char))
            {
                const size_t start = lexer->pos;
                size_t pos = 0;
                char buffer[BUFSIZE];

                do
                {
                    buffer[pos++] = (unsigned char) lexer->cur_char;
                    lexer_advance(lexer);
                } while(isalnum(lexer->cur_char) && pos < BUFSIZE - 1);

                buffer[pos] = 0;

                TokenType type = TOK_ID;
                for(size_t i = 0; i < ARRAY_LEN(keywords); ++i)
                {
                    if(0 == strcmp(keywords[i].lexeme, buffer))
                    {
                        type = keywords[i].type;
                        break;
                    }
                }

                tok = token_new(lexer->arena, type, buffer, start);
            }
        } break;
    }

    lexer->last_tok = lexer->cur_tok;
    lexer->cur_tok = tok;
    return tok;
}

Lexer* lexer_new(const char *source_code)
{
    Lexer *lexer = NULL;

    lexer = calloc(1, sizeof(Lexer));
    if (!lexer) goto err;

    lexer->arena = arena_new(1024);
    if (!lexer->arena) goto err;

    lexer->input = arena_strdup(lexer->arena, source_code);
    if (!lexer->input) goto err;

    lexer_advance(lexer);
    lexer_next_tok(lexer);

    return lexer;

err:
    if (lexer)
    {
        if (lexer->arena) arena_free(lexer->arena);
        free(lexer);
    }

    return NULL;
}

void lexer_free(Lexer *lexer)
{
    arena_free(lexer->arena);
    free(lexer);
}

bool match(Lexer *lexer, const TokenType type)
{
    if(lexer->cur_tok.type == type)
    {
        lexer_next_tok(lexer);
        return true;
    }

    return false;
}

bool is_end(const Token token)
{
    return token.type == TOK_INVALID || token.type == TOK_EOF;
}

Op op_new(const OpCode code, const char *lexeme)
{
    return (Op) {
        .code = code,
        .lexeme = lexeme ? strdup(lexeme) : NULL,
    };
}

char* op_to_str(const Op op)
{
    switch(op.code)
    {
    #define X(a) case a: return #a;
        OP_LIST
    #undef X
    default: return NULL;
    }
}

#define error(input, caret_pos, ...) \
    do { \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\"%s\"\n", input); \
        fprintf(stderr, "%*s^\n", (int) caret_pos, ""); \
        exit(1); \
    } while(0)

void parse_unary(Lexer *lexer, Ops *ops);
void parse_and(Lexer *lexer, Ops *ops);
void parse_or(Lexer *lexer, Ops *ops);

void parse_primary(Lexer *lexer, Ops *ops)
{
    if(match(lexer, TOK_ID))
    {
        const Op op = op_new(OP_ID, lexer->last_tok.lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_TAG))
    {
        const Op op = op_new(OP_TAG, lexer->last_tok.lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_TAGGED))
    {
        const Op op = op_new(OP_TAGGED, lexer->last_tok.lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_UNTAGGED))
    {
        const Op op = op_new(OP_UNTAGGED, lexer->last_tok.lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_LPAREN))
    {
        const Token lparen = lexer->last_tok;
        parse_or(lexer, ops);
        if(!match(lexer, TOK_RPAREN))
        {
            fprintf(stderr, "ERROR: non matching parenthesis!\n");
            fprintf(stderr, "\"%s\"\n", lexer->input);
            const size_t last_tok_len = lexer->last_tok.lexeme ? strlen(lexer->last_tok.lexeme) : 1;
            const size_t caret1 = lparen.pos;
            const size_t caret2 = lexer->last_tok.pos - caret1 + last_tok_len - 1;
            fprintf(stderr, "%*s^%*s^\n", (int) caret1, "", (int) caret2, "");
            exit(1);
        }

        return;
    }

    error(lexer->input, lexer->cur_tok.pos, "ERROR: expected primary expression: ID, Keyword or '('\n");
}

void parse_unary(Lexer *lexer, Ops *ops)
{
    if(match(lexer, TOK_NOT))
    {
        parse_unary(lexer, ops);
        const Op op = op_new(OP_NOT, NULL);
        da_append(ops, op);
        return;
    }

    parse_primary(lexer, ops);
}

void parse_and(Lexer *lexer, Ops *ops)
{
    parse_unary(lexer, ops);
    while(match(lexer, TOK_AND))
    {
        if (is_end(lexer->cur_tok)) error(lexer->input, lexer->last_tok.pos, "ERROR: expected expression after \"and\"\n");

        parse_unary(lexer, ops);
        const Op op = op_new(OP_AND, NULL);
        da_append(ops, op);
    }
}

void parse_or(Lexer *lexer, Ops *ops)
{
    parse_and(lexer, ops);
    while(match(lexer, TOK_OR))
    {
        if (is_end(lexer->cur_tok)) error(lexer->input, lexer->last_tok.pos, "ERROR: expected expression after \"or\"\n");

        parse_and(lexer, ops);
        const Op op = op_new(OP_OR, NULL);
        da_append(ops, op);
    }
}

void parse_expr(Lexer *lexer, Ops *ops)
{
    parse_or(lexer, ops);
    if (!is_end(lexer->cur_tok))
    {
        error(lexer->input, lexer->cur_tok.pos, "ERROR: unexpected token \"%s%s\" at end of expression\n", 
                (lexer->cur_tok.type == TOK_TAG) ? "." : "",
                lexer->cur_tok.lexeme);
    }
}
