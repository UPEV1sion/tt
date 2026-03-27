#include <time.h>
#include <stdio.h>
#include <errno.h>
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

#define DA_GROW_SIZE 2
#define DA_INIT_CAP 64

#define da_append(da, item) \
    do { \
        if((da)->count >= (da)->capacity) \
        { \
            if((da)->capacity == 0) (da)->capacity = DA_INIT_CAP; \
            while((da)->count >= (da)->capacity) (da)->capacity *= DA_GROW_SIZE; \
            (da)->items = realloc((da)->items, (da)->capacity); \
            assert((da)->items, "ERROR: out of mem!\n"); \
        } \
        (da)->items[(da)->count++] = item; \
    } while(0)

#define da_foreach(type, name, da) for(type *name = (da)->items; name < (da)->items + (da)->count; ++name)

typedef enum {
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_GROUPING,
    OP_TAG,
    OP_ID
} OpCode;

typedef struct {
    OpCode op;
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
    Token **items;
} Ops;

typedef struct {
    size_t pos;
    const char *input;
    Token *last_tok;
    int last_char;
} Lexer;

Token* token_new(const TokenType type, const char *lexeme, const size_t n, const size_t pos)
{
    Token *tok = malloc(sizeof(Token));
    assert(tok != NULL, "ERROR: Count not create token!\n");
    tok->type = type;
    tok->lexeme = strndup(lexeme, n);
    tok->pos = pos;
    return tok;
}

void print_usage(const char *program)
{
    printf("USAGE: %s <flags>\n", program);
    printf("    -n, --new:       create a new task\n");
    printf("    -f, --filter:    filter existing tasks\n");
}

void current_timestamp(char *buffer, size_t buffer_size)
{
    time_t t = time(NULL);
    assert(t > -1, strerror(errno));
    struct tm *lt = localtime(&t);
    assert(lt != NULL, strerror(errno));
    assert(strftime(buffer, buffer_size, "%F-%H-%M-%S", lt) > 0, "ERROR: could not format time!\n");
}

bool match(Lexer *lexer, TokenType type)
{
    if(lexer->cur_tok->type == type)
    {
        lexer_next_tok(lexer);
        return true;
    }

    return false;
}

Token* parse_primary(Lexer *lexer, Ops *ops)
{
    if(match(lexer, TOK_ID)) da_append(ops, lexer->last_tok);
    if(

}

Token* parse_and(Lexer *lexer, Ops *ops)
{
    Token *cur_tok = parse_primary(lexer, ops);
    while(match(lexer, TOK_AND))
    {

    }

    return cur_tok;
}

Token* parse_or(Lexer *lexer, Ops *ops)
{
    Token *cur_tok = parse_and(lexer, ops);
    while(match(lexer, TOK_OR))
    {

    }

    return cur_tok;
}

void parse_expr(Lexer *lexer, Ops *ops)
{
    parse_or(lexer, ops);
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
            Lexer lexer = { .input = filter };
            Ops ops = {0};
            da_append(&ops, token_new(TOK_AND, "and", 3, 0));
            da_foreach(Token*, tok, &ops)
            {
                printf("%s:%zu", (*tok)->lexeme, (*tok)->pos);
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
