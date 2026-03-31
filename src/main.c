//
// Created by escha on 26.03.26.
//

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#define DA_IMPLEMENTATION
#include "da.h"
#include "parser.h"
#include "common.h"

#define shift_args(argc, argv)((argc)--, *(argv)++)

typedef struct {
    size_t count;
    size_t capacity;
    StringView *items;
} Tags;

typedef enum {
    OPEN,
    IN_PROGRESS,
    CLOSED,
    status_count_,
} TaskStatus;

typedef struct {
    StringView id;
    StringView title;
    TaskStatus status;
    long prio;
    Tags tags;
} Task;

typedef struct {
    size_t count;
    size_t capacity;
    Task *items;
} Tasks;

typedef struct {
    size_t count;
    size_t capacity;
    bool *items;
} Stack;

typedef enum {
    SORT_TIMESTAMP,
    SORT_PRIO,
} SortTarget;

typedef struct {
    SortTarget target;
    bool desc;
} SortOptions;

void current_timestamp(char *buffer, const size_t buffer_size)
{
    const time_t t = time(NULL);
    assertmsg(t > -1, "Could not get time: %s\n", strerror(errno));
    const struct tm *lt = localtime(&t);
    assertmsg(lt != NULL, "Could not get localtime: %s\n", strerror(errno));
    assertmsg(strftime(buffer, buffer_size, "%Y%m%d-%H%M%S", lt) > 0, "ERROR: could not format time!\n");
}

Task parse_task(const StringView id)
{
    char path[BUFSIZE];
    snprintf(path, BUFSIZE, "./tasks/"SV_FMT"/TASK.md", SV_ARG(id));

    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    Task task = {0};
    const StringView title = sv_chop(&sv, '\n');
    task.title = title;
    task.id = id;
    sv_chop(&sv, '\n');

    StringView meta;
    while((meta = sv_chop(&sv, '\n')).len > 0)
    {
        if(sv_is_prefix(meta, "- PRIORITY"))
        {
            sv_chop(&meta, ':');
            char prio_buf[BUFSIZE];
            size_t read = 0;
            size_t write = 0;
            const size_t len = meta.len < BUFSIZE ? meta.len : BUFSIZE;
            while (isspace(meta.s[read])) read++;
            while (write < len - 1 && isdigit(meta.s[read]))
            {
                prio_buf[write++] = meta.s[read++];
            }
            prio_buf[write] = 0;

            char *endptr;
            const long num = strtol(prio_buf, &endptr, 10);
            assertmsg(*endptr == 0, "ERROR: count not parse priority: %s\n", prio_buf);

            task.prio = num;
        }
        else if(sv_is_prefix(meta, "- STATUS"))
        {
            sv_chop(&meta, ':');
            const StringView trimmed = sv_trim(meta);
            if(trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"STATUS\" header in %s\n", path);
                exit(1); 
            }

            static_assert(status_count_ == 3, "ERROR: non exhausive handling of STATUSES");
            if(sv_equal(trimmed, sv_from_cstr("OPEN")))
            {
                task.status = OPEN;
            }
            else if(sv_equal(trimmed, sv_from_cstr("IN_PROGRESS")))
            {
                task.status = IN_PROGRESS;
            }
            else if(sv_equal(trimmed, sv_from_cstr("CLOSED")))
            {
                task.status = CLOSED;
            }
            else
            {
                fprintf(stderr, "ERROR: Malfromed \"STATUS\" header in %s\n", path);
                fprintf(stderr, "ERROR: Available options: \"OPEN\", \"IN_PROGRESS\", \"CLOSED\"\n");
                exit(1);
            }
        }
        else if(sv_is_prefix(meta, "- TAGS"))
        {
            sv_chop(&meta, ':');
            StringView trimmed = sv_trim(meta); 
            if(trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"TAGS\" header in %s\n", path);
                exit(1);
            }

            StringView tag;
            while((tag = sv_chop(&trimmed, ',')).len > 0)
            {
                tag = sv_trim(tag);
                da_append(&task.tags, tag);
            }
        }
        else if(*meta.s == '-')
        {
            fprintf(stderr, "ERROR: Invalid task header \""SV_FMT"\" in %s\n", SV_ARG(meta), path);
            exit(1);
        }
        else if(*meta.s == '\n')
        {
            break;
        }
    }

    return task;
}

void create_task_folder(void)
{
    DIR *dir = opendir("./tasks");
    if(dir == NULL)
    {
        fprintf(stderr, "ERROR: No 'tasks' folder found!\n");
        fprintf(stderr, "INFO: Creating new 'tasks' folder\n");
        if(mkdir("./tasks", 0755) != 0)
        {
            fprintf(stderr, "ERROR: Could not create 'tasks' folder: %s", strerror(errno));
            exit(1);
        }

        errno = 0;
        return;
    }
    closedir(dir);
}

void load_tasks(Tasks *tasks)
{
    create_task_folder();

    DIR *dir = opendir("./tasks");
    if(dir == NULL)
    errno = 0;
    struct dirent *dir_entry;
    while((dir_entry = readdir(dir)) != NULL)
    {
        if (0 == strcmp(dir_entry->d_name, ".") || 0 == strcmp(dir_entry->d_name, "..")) continue;

        assertmsg(dir_entry->d_type == DT_DIR, 
                "ERROR: malformed \"tasks\" folder. Please remove \"./tasks/%s\"!\n", dir_entry->d_name);
        const StringView id = sv_from_cstr(dir_entry->d_name);
        const Task task = parse_task(id);
        if(task.status != CLOSED) da_append(tasks, task);
    }

    assertmsg(closedir(dir) == 0, "ERROR: Could not close \"tasks\" folder: %s\n", strerror(errno));
    assertmsg(errno == 0, "ERROR: Could not properly read \"tasks\" folder: %s\n", strerror(errno));
}

bool task_contains_tag(const Task task, const char *tag)
{
    da_foreach(StringView, sv, &task.tags)
    {
        char *cur_tag = cstr_from_sv(*sv);
        if(0 == strcmp(cur_tag, tag)) 
        {
            free(cur_tag);
            return true;
        }
        free(cur_tag);
    }
    
    return false;
}

bool task_matches_filter(const Task task, const Ops *ops, Stack *stack)
{
    da_foreach(Op, op, ops)
    {
        switch(op->code)
        {
            case OP_AND: {
                assertmsg(stack->count >= 2, "ERROR: stack underflow\n");
                const bool a = stack->items[stack->count - 1];
                const bool b = stack->items[stack->count - 2];
                stack->count -= 2;
                da_append(stack, a && b);
            } break;
            case OP_OR: {
                assertmsg(stack->count >= 2, "ERROR: stack underflow\n");
                const bool a = stack->items[stack->count - 1];
                const bool b = stack->items[stack->count - 2];
                stack->count -= 2;
                da_append(stack, a || b);
            } break;
            case OP_NOT: {
                assertmsg(stack->count >= 1, "ERROR: stack underflow\n");
                stack->items[stack->count - 1] = !stack->items[stack->count - 1];
            } break;
            case OP_TAG: {
                const bool a = task_contains_tag(task, op->lexeme);
                da_append(stack, a);
            } break;
            case OP_TAGGED: {
                da_append(stack, task.tags.count > 0);
            } break;
            case OP_UNTAGGED: {
                da_append(stack, task.tags.count == 0);
            } break;
            case OP_ID: assertmsg(false, "TODO"); break;
            default: assertmsg(false, "UNREACHABLE\n"); break;
        }
    }

    assertmsg(stack->count == 1, "ERROR: invalid filter evaluation\n");
    return *stack->items;
}

void filter_tasks(Tasks *tasks, const Ops *ops)
{
    Stack stack = {0};
    for(size_t i = tasks->count; i > 0; --i)
    {
        stack.count = 0;
        if(!task_matches_filter(tasks->items[i - 1], ops, &stack))
        {
            da_remove_unordered(tasks, i - 1);
        }
    }
}

void dump_tasks(const Tasks *tasks)
{
    da_foreach(Task, task, tasks)
    {
        printf("./tasks/"SV_FMT"/TASK.md: [PRIORITY: %ld", SV_ARG(task->id), task->prio);
        if(task->tags.count > 0) 
        {
            printf(", TAGS: ");
            printf(SV_FMT, SV_ARG(*task->tags.items));
            for(size_t i = 1; i < task->tags.count; ++i)
            {
                printf(", "SV_FMT, SV_ARG(task->tags.items[i]));
            } 
        }
        assertmsg(task->title.len >= 2, "ERROR: malformed title\""SV_FMT"\"\n", SV_ARG(task->title));
        const StringView tmp_title = {.s = task->title.s + 2, .len = task->title.len - 2};
        printf("] "SV_FMT"\n", SV_ARG(tmp_title));
    }
}

void create_new_task(void)
{
    char timestamp[128];
    current_timestamp(timestamp, sizeof timestamp);
    printf("INFO: Creating \"./tasks/%s/TASK.md\"\n", timestamp);
    printf("INFO: Please provide the following information\n");

    char title[BUFSIZE];
    printf("Title: ");
    assertmsg(fgets(title, sizeof title, stdin) != NULL, "ERROR: Could not read title\n");
    title[strcspn(title, "\n")] = 0;

    char status[BUFSIZE];
    static_assert(status_count_ == 3, "ERROR: non exhausive handling of status");
    printf("Status (OPEN, IN_PROGRESS, CLOSED): ");
    assertmsg(fgets(status, sizeof status, stdin) != NULL, "ERROR: Could not read status\n");
    status[strcspn(status, "\n")] = 0;
    char *states[] = {"OPEN", "IN_PROGRESS", "CLOSED"};
    bool valid_state = false;
    for(size_t i = 0; i < ARRAY_LEN(states); ++i)
    {
        valid_state = valid_state || (0 == strcmp(states[i], status));
    }
    if(!valid_state)
    {
        fprintf(stderr, "ERROR: invalid status \"%s\"\n", status);
        fprintf(stderr, "ERROR: Available options: \"OPEN\", \"IN_PROGRESS\", \"CLOSED\"\n");
        exit(1);
    }

    long prio = 0;
    printf("Priority (0-100): ");
    assertmsg(scanf("%ld", &prio) == 1, "ERROR: Could not read priority\n");
    assertmsg(prio >= 0 && prio <= 100, "ERROR: Invalid priority \"%ld\". Available range 0-100\n", prio);
    getchar();

    char tags[BUFSIZE];
    printf("Tags (optional, comma separated): ");
    assertmsg(fgets(tags, sizeof tags, stdin) != NULL, "ERROR: Could not read tags\n");
    tags[strcspn(tags, "\n")] = 0;

    create_task_folder();
    char path[BUFSIZE];
    const int offset = snprintf(path, BUFSIZE, "./tasks/%s/", timestamp);
    if(mkdir(path, 0755) != 0)
    {
        fprintf(stderr, "ERROR: Could not create \"%s\" folder: %s\n", path, strerror(errno));
        exit(1);
    }
    
    snprintf(path + offset, BUFSIZE - offset, "TASK.md"); 
    FILE *fp;
    assertmsg((fp = fopen(path, "w")) != NULL, "ERROR: Could not open \"%s\" for writing\n", path);

    fprintf(fp, "# %s\n\n", title);
    fprintf(fp, "- STATUS: %s\n", status);
    fprintf(fp, "- PRIORITY: %ld\n", prio);
    if(strlen(tags) > 0) fprintf(fp, "- TAGS: %s", tags);

    fclose(fp);

    printf("INFO: Wrote \"%s\"\n", path);
}

void print_usage(const char *program)
{
    printf("USAGE: %s <flags>\n", program);
    printf("    -h, --help:      print help\n");
    printf("    -n, --new:       create a new task\n");
    printf("    -d, --date:      sort by creation date (descending)\n");
    printf("    -D, --Date:      sort by creation date (ascending)\n");
    printf("    -p, --priority:  sort by priority (descending, default)\n");
    printf("    -P, --Priority:  sort by priority (ascending)\n");
    printf("    -f, --filter:    filter existing tasks\n");
    printf("        syntax: '.<tag>', 'and', 'or', 'not', 'tagged', untagged, '(' and ')'\n");
    printf("        example: -f \".bug or untagged\"\n");
    printf("        example: -f \".unfinished and not (.feature or .refactor)\"\n");
}

static SortOptions sort_options = {.desc = true, .target = SORT_PRIO};

int task_cmp(const void *a, const void *b)
{
    const Task *task_a = (const Task *)a;
    const Task *task_b = (const Task *)b;
    switch(sort_options.target)
    {
        case SORT_PRIO: {
            if (sort_options.desc)
            {
                return (task_b->prio > task_a->prio) - (task_b->prio < task_a->prio);
            }
            return (task_a->prio > task_b->prio) - (task_a->prio < task_b->prio);
        } break;
        case SORT_TIMESTAMP: {
            if (sort_options.desc)
            {
                return sv_cmp(task_a->id, task_b->id);
            }
            else
            {
                return sv_cmp(task_b->id, task_a->id);
            }
        } break;
        default: assertmsg(false, "UNREACHABLE");
    }
}

int main(int argc, char **argv)
{
    const char *program = shift_args(argc, argv);
    Tasks tasks = {0};
    load_tasks(&tasks);

    // TASK(20260328-085723)
    // TASK(20260328-090107)
    while(argc > 0)
    {
        const char *flag = shift_args(argc, argv);
        if(0 == strcmp("-n", flag) || 0 == strcmp("--new", flag))
        {
            create_new_task();
            return 0;
        }
        else if(0 == strcmp("-f", flag) || 0 == strcmp("--filter", flag))
        {
            const char *filter = shift_args(argc, argv);
            assertmsg(argc >= 0, "ERROR: no filter provided!\n");
            Lexer *lexer = lexer_new(filter);
            Ops ops = {0};
            parse_expr(lexer, &ops);
            filter_tasks(&tasks, &ops);
        }
        else if(0 == strcmp("-d", flag) || 0 == strcmp("--date", flag))
        {
            sort_options.desc = true;
            sort_options.target = SORT_TIMESTAMP;
        }
        else if(0 == strcmp("-D", flag) || 0 == strcmp("--Date", flag))
        {
            sort_options.desc = false;
            sort_options.target = SORT_TIMESTAMP;
        }
        else if(0 == strcmp("-p", flag) || 0 == strcmp("--priority", flag))
        {
            sort_options.desc = true;
            sort_options.target = SORT_PRIO;
        }
        else if(0 == strcmp("-P", flag) || 0 == strcmp("--Priority", flag))
        {
            sort_options.desc = false;
            sort_options.target = SORT_PRIO;
        }
        else if(0 == strcmp("-h", flag) || 0 == strcmp("--help", flag))
        {
            print_usage(program);
            return 0;
        }
        else
        {
            fprintf(stderr, "ERROR: Invalid flag \"%s\"\n", flag);
            print_usage(program);
            return 1;
        }
    }

    da_sort(&tasks, task_cmp);
    dump_tasks(&tasks);

    return 0;
}
