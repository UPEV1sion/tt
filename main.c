#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    size_t count;
    size_t capacity;
    char *items;
} StringBuilder;

typedef struct {
    const char *s;
    size_t len;
} StringView;

typedef struct {
    size_t count;
    size_t capacity;
    StringView *items;
} Tags;

typedef struct {
    StringView title;
    StringView status;
    long prio;
    Tags tags;
} Task;

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
    TOK_TAGGED,
    TOK_UNTAGGED,
    TOK_ID,
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

#define DA_GROW_SIZE 2
#define DA_INIT_CAP 64

#define da_reserve(da, expected) \
    do { \
        if((expected) > (da)->capacity) \
        { \
            if((da)->capacity == 0) (da)->capacity = DA_INIT_CAP; \
            while((expected) > (da)->capacity) (da)->capacity *= DA_GROW_SIZE; \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items, "ERROR: out of mem!\n"); \
        } \
    while(0)

#define da_append(da, item) \
    do { \
        da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item); \
    } while(0)

#define sb_append(sb, s) \
    do { \
        const size_t len = strlen(s); \
        da_reserve((sb), (sb)->count + len); \
        memcpy((da)->items + (da)->count, (s), len); \
    while(0)

#define da_foreach(type, name, da) for(type *name = (da)->items; name < (da)->items + (da)->count; ++name)

StringView sv_from_sb(const StringBuilder *sb)
{
    return (StringView) {
        .s = sb->items,
        .len = sb->count
    };
}

StringView sv_chop(StringView *sv, const int delim)
{
    if(sv->len == 0) return (StringView) {NULL, 0};

    size_t i;
    for(i = 0; i < sv->len && sv->s[i] != (unsigned char) delim; ++i)
        ;

    const StringView ret = {sv->s, i}; 
    sv->s += i;
    sv->len -= i;

    return ret;
}

bool sv_is_prefix(StringView sv, const char *prefix)
{
    const size_t len = strlen(prefix);
    if(sv.len < len) return false;

    for(size_t i = 0; i < len; ++i)
    {
        if(sv.s[i] != prefix[i]) return false;
    }

    return true;
}

StringView sv_trim(StringView sv)
{
    while(sv.len > 0 && isspace((unsigned char) *sv.s))
    {
        sv.s++;
        sv.len--;
    }

    while(sv.len > 0 && isspace((unsigned char) sv.s[sv.len]))
    {
        sv.len--;
    }

    return sv;
}

char* cstr_from_sv(const StringView sv)
{
    char *str;
    assert((str = malloc(sv.len + 1)) != NULL, "Could not allocate cstr\n");
    memcpy(str, sv.s, sv.len);
    str[sv.len] = 0;
    return str;
}

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

                for(size_t i = 0; i < ARRAY_LEN(keywords); ++i)
                {
                    if(0 == strcmp(keywords[i].lexeme, buffer))
                    {
                        tok = token_new(keywords[i].type, buffer, lexer->pos);
                        goto end;
                    }
                }

                if(*buffer == '.')
                {
                    tok = token_new(TOK_TAG, buffer, lexer->pos);
                }
                else
                {
                    tok = token_new(TOK_ID, buffer, lexer->pos);
                }
            }
        } break;
    }

end:
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
    printf("    -h, --help:      print help\n");
    printf("    -n, --new:       create a new task\n");
    printf("    -d, --date:      sort by creation date\n");
    printf("    -p, --priority:  sort by priority\n");
    printf("    -f, --filter:    filter existing tasks\n");
    printf("        syntax: '.<tag>', 'and', 'or', 'not', 'tagged', untagged, '(' and ')'\n");
    printf("        example: -f \".bug or untagged\"\n");
    printf("        example: -f \".unfinished and not (.feature or .refactor)\"\n");
}

void current_timestamp(char *buffer, const size_t buffer_size)
{
    const time_t t = time(NULL);
    assert(t > -1, "Could not get time: %s\n", strerror(errno));
    const struct tm *lt = localtime(&t);
    assert(lt != NULL, "Could not get localtime: %s\n", strerror(errno));
    assert(strftime(buffer, buffer_size, "%Y%m%d-%H%M%S", lt) > 0, "ERROR: could not format time!\n");
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

    if(match(lexer, TOK_TAGGED))
    {
        Op *op = op_new(OP_TAGGED, lexer->last_tok->lexeme);
        da_append(ops, op);
        return;
    }

    if(match(lexer, TOK_UNTAGGED))
    {
        Op *op = op_new(OP_UNTAGGED, lexer->last_tok->lexeme);
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
            const size_t caret2 = lexer->last_tok->pos - caret1 - 1;
            fprintf(stderr, "%*s^%*s^\n", (int) caret1, "", (int) caret2, "");
            exit(1);
        }

        return;
    }

    fprintf(stderr, "ERROR: expected primary expression: ID, Keyword or '('\n");
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

int read_file(StringBuilder *sb, const char *path)
{
    FILE *f;
    assert((f = fopen(path, "r")) != NULL, "Could not open file %s\n", path);
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    da_reserve(sb, sb->count + size);
    fread(sb->items + sb->count, size, 1, f);
    assert(ferror(f) == 0, "ERROR: Could not read file %s\n", path);
    fclose(f);

    return 0;
}

Task parse_task(const char *path)
{
    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    Task task = {0};
    StringView title = sv_chop(&sv, '\n');
    sv_chop(&sv, '\n');

    StringView meta;
    while((meta = sv_chop(&sv, '\n')).len > 0)
    {
        if(sv_is_prefix(meta, "- PRIORITY"))
        {
            StringView prio = sv_chop(&meta, ':');
            prio = sv_trim(prio);
            char *prio_str = cstr_from_sv(prio);

            char *endptr;
            const long num = strtol(prio_str, &endptr, 10);
            assert(*endptr == 0, "ERROR: count not parse priority: %s\n", prio_str);

            task.prio = num;
            free(prio_str);
        }
        else if(sv_is_prefix(meta, "- STATUS"))
        {
            task.status = meta;
        }
        else if(sv_is_prefix(meta, "- TAGS"))
        {
            sv_chop(&meta, ':');
            StringView trimmed = sv_trim(meta); 
            if(trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed 'TAGS' header in %s\n", path);
                exit(1);
            }

            StringView tag;
            while((tag = sv_chop(&trimmed, ',')).len > 0)
            {
                tag = sv_trim(tag);
                da_append(&task.tag, tag);
            }
        }
        else if(*meta.s == '-')
        {
            fprintf(stderr, "ERROR: Invalid task header \"%.*s\" in %s\n", meta.len, meta.s, path);
            exit(1);
        }
        else if(*meta.s == '\n')
        {
            break;
        }
    }

    return task;
}

void load_tasks(Tasks *tasks)
{
    const DIR *dir = opendir("./tasks");
    if(dir == NULL)
    {
        fprintf(stderr, "ERROR: No 'tasks' folder found!\n");
        fprintf(stderr, "INFO: Creating new 'tasks' folder\n");
        if(mkdir("./tasks", 0755) != 0)
        {
            fprintf(stderr, "ERROR: Could not create 'tasks' folder: %s", strerror(errno));
            exit(1);
        }

        return;
    }

    errno = 0;
    struct dirent dir_entry;
    while((dir_entry = readdir(dir)) != NULL)
    {
        assert(dir_entry->d_type == DT_DIR, 
                "ERROR: malformed 'tasks' folder. Please remove './tasks/%'!\n", dir_entry->d_name);
        char path[BUFSIZE];
        snprintf(path, BUFSIZE, "./tasks/%s/TASK.md", dir_entry->d_name);
        Task task = parse_task(path);
        da_append(task, tasks);
    }

    assert(closedir(dir) == 0, "ERROR: Could not close 'tasks' folder: %s\n", strerror(errno));    
    assert(errno == 0, "ERROR: Could not properly read 'tasks' folder: %s\n", strerror(errno));
}

void dump_tasks(void)
{
    Tasks tasks = {0};
    load_tasks(&tasks);
    da_sort(&tasks);
    da_foreach(Task, task, &tasks)
    {
        printf("%s: [PRIORITY: %d", task->path, task->priority);
        if(task->tags.count > 0) 
        {
            printf(", TAGS: ");
            printf("%s", *task->tags->items);
            for(size_t i = 1; i < task->tags.count; ++i)
            {
                printf(", %s", task->tags.items[i]);
            } 
        }
        printf("] %s", task->description);
    }
}

int main(int argc, char **argv)
{
    const char *program = shift_args(argc, argv);
    if(argc <= 0)
    {
        dump_tasks();
        return 0;
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
                printf("%s", op_to_str(*op));
                if((*op)->code == OP_TAG) printf(": %s", (*op)->lexeme);
                puts("");
            }
        }
        else if(0 == strcmp("-d", flag) || 0 == strcmp("--date", flag))
        {
            assert(false, "TODO");
        }
        else if(0 == strcmp("-p", flag) || 0 == strcmp("--priority", flag))
        {
            assert(false, "TODO");
        }
        else if(0 == strcmp("-h", flag) || 0 == strcmp("--help", flag))
        {
            print_usage();
        }
        else
        {
            fprintf(stderr, "ERROR: Invalid flag \"%s\"\n", flag);
            print_usage(program);
        }
    }

    return 0;
}

