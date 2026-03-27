#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define shift_args(argc, argv)((argc)--, *(argv)++)
#define ARRAY_LEN(a)(sizeof(a)/sizeof(a[0]))
#define assert(cond, ...) \
    do { \
        if(!(cond)) \
        { \
            fprintf(stderr, __VA_ARGS__); \
            exit(1); \
        } \
    } while(0)

#define BUFSIZE 1024

#define DA_GROW_SIZE 2
#define DA_INIT_CAP 64

#define da_append(da, item) \
    do { \
        if((da)->count >= (da)->capacity) \
        { \
            if((da)->capacity == 0) (da)->capacity = DA_INIT_CAP; \
            while((da)->count >= (da)->capacity) (da)->capacity *= DA_GROW_SIZE; \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items, "ERROR: out of mem!\n"); \
        } \
        (da)->items[(da)->count++] = item; \
    } while(0)

#define da_foreach(type, name, da) for(type *name = (da)->items; name < (da)->items + (da)->count; ++name)

#define OP_LIST\
    X(OP_AND) \
    X(OP_OR) \
    X(OP_NOT) \
    X(OP_TAG) \
    X(OP_ID)

typedef enum {
#define X(a) a,
    OP_LIST
#undef X
} OpCode;

typedef struct {
    OpCode code;
    const char *lexeme;
} Op;

typedef enum {
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_TAG,
    TOK_ID,
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    size_t pos;
} Token;

typedef struct {
    size_t count;
    size_t capacity;
    Op **items;
} Ops;

typedef struct {
    size_t pos;
    const char *input;
    Token *cur_tok;
    Token *last_tok;
    int cur_char;
} Lexer;

Token* token_new(const TokenType type, const char *lexeme, const size_t pos)
{
    Token *tok = malloc(sizeof(Token));
    assert(tok != NULL, "ERROR: Count not create token!\n");
    tok->type = type;
    tok->lexeme = strdup(lexeme);
    tok->pos = pos;

    return tok;
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
    assert(lexer->cur_char == c, "ERROR: unexpected char '%c'\n", c);
    lexer_advance(lexer);
}

Token* lexer_next_tok(Lexer *lexer)
{
    lexer_trim_left(lexer);

    Token *tok = NULL;
    switch(lexer->cur_char)
    {
        case '(': {
            tok = token_new(TOK_LPAREN, "(", lexer->pos);
            lexer_advance(lexer);
        } break;
        case ')': {
            tok = token_new(TOK_RPAREN, ")", lexer->pos);
            lexer_advance(lexer);
        } break;
        case '!': {
            tok = token_new(TOK_NOT, "!", lexer->pos);
            lexer_advance(lexer);
        }  break;
        case '&': {
            const size_t pos = lexer->pos;
            lexer_match_char(lexer, '&');
            tok = token_new(TOK_AND, "&&", pos);
            lexer_advance(lexer);
        } break;
        case '|': {
            const size_t pos = lexer->pos;
            lexer_match_char(lexer, '|');
            tok = token_new(TOK_OR, "||", pos);
            lexer_advance(lexer);
        } break;
        default : {
            if(lexer->cur_char == '.' || isalpha(lexer->cur_char))
            {
                size_t pos = 0;
                char buffer[BUFSIZE];

                do
                {
                    buffer[pos++] = (unsigned char) lexer->cur_char;
                    lexer_advance(lexer);
                } while(isalnum(lexer->cur_char) && pos < BUFSIZE - 1);

                buffer[pos] = 0;
                tok = token_new((buffer[0] == '.') ? TOK_TAG : TOK_ID, buffer, lexer->pos);
            }
        } break;
    }

    lexer->last_tok = lexer->cur_tok;
    lexer->cur_tok = tok;
    return tok;
}

Lexer *lexer_new(const char *source_code)
{
    Lexer *lexer = calloc(1, sizeof(Lexer));
    assert(lexer != NULL, "ERROR: Could not allocate lexer\n");
    lexer->input = strdup(source_code);
    assert(lexer->input != NULL, "Could not allocate source string\n");
    lexer_advance(lexer);
    lexer_next_tok(lexer);

    return lexer;
}

Op* op_new(const OpCode code, const char *lexeme)
{

    Op *op = malloc(sizeof(Op));
    assert(op != NULL, "ERROR: Count not create op!\n");
    op->code = code;
    op->lexeme = lexeme != NULL ? strdup(lexeme) : NULL;

    return op;
}

char* op_to_str(const Op *op)
{
    switch(op->code)
    {
#define X(a) case a: return #a;
        OP_LIST
#undef X
    default: return NULL;
    }
}

void print_usage(const char *program)
{
    printf("USAGE: %s <flags>\n", program);
    printf("    -n, --new:       create a new task\n");
    printf("    -f, --filter:    filter existing tasks\n");
}

void current_timestamp(char *buffer, size_t buffer_size)
{
    const time_t t = time(NULL);
    assert(t > -1, "Could not get time: %s\n", strerror(errno));
    const struct tm *lt = localtime(&t);
    assert(lt != NULL, "Could not get localtime: %s\n", strerror(errno));
    assert(strftime(buffer, buffer_size, "%F-%H-%M-%S", lt) > 0, "ERROR: could not format time!\n");
}

bool match(Lexer *lexer, const TokenType type)
{
    if (lexer->cur_tok == NULL) return false;

    if(lexer->cur_tok->type == type)
    {
        lexer_next_tok(lexer);
        return true;
    }

    return false;
}

void parse_primary(Lexer *lexer, Ops *ops);
void parse_unary(Lexer *lexer, Ops *ops);
void parse_and(Lexer *lexer, Ops *ops);
void parse_or(Lexer *lexer, Ops *ops);

void parse_primary(Lexer *lexer, Ops *ops)
{
    if(match(lexer, TOK_ID))
    {
        Op *op = op_new(OP_ID, lexer->last_tok->lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_TAG))
    {
        Op *op = op_new(OP_TAG, lexer->last_tok->lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_LPAREN))
    {
        Token *lparen = lexer->last_tok;
        parse_or(lexer, ops);
        if(!match(lexer, TOK_RPAREN))
        {
            fprintf(stderr, "ERROR: non matching parenthesis!\n");
            fprintf(stderr, "\"%s\"\n", lexer->input);
            const size_t caret1 = lparen->pos;
            const size_t caret2 = lexer->last_tok->pos - caret1;
            fprintf(stderr, "%*s^%*s^\n", (int) caret1, "", (int) caret2, "");
            exit(1);
        }

        return;
    }

    fprintf(stderr, "ERROR: expected primary expression: ID, TAG or '('\n");
    fprintf(stderr, "\"%s\"\n", lexer->input);
    const size_t caret = lexer->last_tok->pos;
    fprintf(stderr, "%*s^\n", (int) caret, "");
    exit(1);
}

void parse_unary(Lexer *lexer, Ops *ops)
{
    if(match(lexer, TOK_NOT))
    {
        parse_unary(lexer, ops);
        Op *op = op_new(OP_NOT, NULL);
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
        parse_unary(lexer, ops);
        Op *op = op_new(OP_AND, NULL);
        da_append(ops, op);
    }
}

void parse_or(Lexer *lexer, Ops *ops)
{
    parse_and(lexer, ops);
    while(match(lexer, TOK_OR))
    {
        parse_and(lexer, ops);
        Op *op = op_new(OP_OR, NULL);
        da_append(ops, op);
    }
}

void parse_expr(Lexer *lexer, Ops *ops)
{
    parse_or(lexer, ops);
     if (lexer->cur_tok != NULL) {
        fprintf(stderr, "ERROR: unexpected token '%s' at end of expression\n", lexer->cur_tok->lexeme);
        fprintf(stderr, "\"%s\"\n", lexer->input);
        fprintf(stderr, "%*s^\n", (int) lexer->last_tok->pos, "");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    const char *program = shift_args(argc, argv);
    if(argc <= 0)
    {
        print_usage(program);
        return 1;
    }

    while(argc > 0)
    {
        const char *flag = shift_args(argc, argv);
        if(0 == strcmp("-n", flag) || 0 == strcmp("--new", flag))
        {
            char timestamp[128];
            current_timestamp(timestamp, sizeof timestamp);
            puts(timestamp);
        }
        else if(0 == strcmp("-f", flag) || 0 == strcmp("--filter", flag))
        {
            const char *filter = shift_args(argc, argv);
            assert(argc >= 0, "ERROR: no filter provided!\n");
            Lexer *lexer = lexer_new(filter);
            Ops ops = {0};
            parse_expr(lexer, &ops);
            da_foreach(Op*, op, &ops)
            {
                printf("%s\n", op_to_str(*op));
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Invalid flag \"%s\"\n", flag);
            print_usage(program);
        }
    }

    return 0;
}

