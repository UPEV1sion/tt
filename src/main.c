//
// Created by escha on 26.03.26.
//

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "ht.h"
#include "da.h"
#include "parser.h"
#include "common.h"

#define shift_args(argc, argv)((argc)--, *(argv)++)

typedef struct {
    size_t count;
    size_t capacity;
    StringView *items;
} Tags;

typedef struct {
    size_t count;
    size_t capacity;
    StringView *items;
} References;

typedef enum {
    OPEN,
    IN_PROGRESS,
    CLOSED,
    status_count_,
} TaskStatus;

typedef struct {
    char *id;
    StringView title;
    TaskStatus status;
    long prio;
    Tags tags;
    References refs;
    StringBuilder backing;
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
    SORT_TOPOLOGICAL,
} SortTarget;

typedef struct {
    SortTarget target;
    bool desc;
} SortOptions;


static const char* status_to_str(const TaskStatus status)
{
    switch (status)
    {
        case OPEN:        return "OPEN";
        case IN_PROGRESS: return "IN_PROGRESS";
        case CLOSED:      return "CLOSED";
        default: assert(0 && "Unreacheable");
    }
}

static void current_timestamp(char *buffer, const size_t buffer_size)
{
    const time_t t = time(NULL);
    assertmsg(t > -1, "Could not get time: %s\n", strerror(errno));
    const struct tm *lt = localtime(&t);
    assertmsg(lt != NULL, "Could not get localtime: %s\n", strerror(errno));
    assertmsg(strftime(buffer, buffer_size, "%Y%m%d-%H%M%S", lt) > 0, "ERROR: could not format time!\n");
}

static Task parse_task(char *id)
{
    char path[BUFSIZE];
    snprintf(path, BUFSIZE, "./tasks/%s/TASK.md", id);

    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    const StringView title = sv_chop(&sv, '\n');
    Task task = {
        .title = title,
        .id = id,
        .backing = sb,
    };
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

            static_assert(status_count_ == 3, "ERROR: non exhaustive handling of STATUSES");
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
        else if(sv_is_prefix(meta, "- REFERENCES"))
        {
            sv_chop(&meta, ':');
            StringView trimmed = sv_trim(meta); 
            if(trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"REFERENCES\" header in %s\n", path);
                exit(1);
            }

            StringView ref;
            while((ref = sv_chop(&trimmed, ',')).len > 0)
            {
                ref = sv_trim(ref);
                da_append(&task.refs, ref);
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

static void create_task_folder(void)
{
    struct stat st;
    if (stat("./tasks", &st) == 0 && S_ISDIR(st.st_mode)) return;

    fprintf(stderr, "ERROR: No \"tasks\" folder found!\n");
    fprintf(stderr, "INFO: Creating new \"tasks\" folder\n");
    if(mkdir("./tasks", 0755) != 0)
    {
        fprintf(stderr, "ERROR: Could not create \"tasks\" folder: %s", strerror(errno));
        exit(1);
    }
}

static void task_free(const Task task)
{
    free(task.id);
    da_free(&task.backing);
    da_free(&task.tags);
    da_free(&task.refs);
}

static void load_tasks(Tasks *tasks)
{
    DIR *dir = opendir("./tasks");
    if(!dir)
    {
        fprintf(stderr, "ERROR: No \"tasks\" folder found!\n");
        return;
    }
    errno = 0;
    struct dirent *dir_entry;
    while((dir_entry = readdir(dir)) != NULL)
    {
        if (0 == strcmp(dir_entry->d_name, ".") || 0 == strcmp(dir_entry->d_name, "..")) continue;

        assertmsg(dir_entry->d_type == DT_DIR, 
                "ERROR: malformed \"tasks\" folder. Please remove \"./tasks/%s\"!\n", dir_entry->d_name);
        const Task task = parse_task(strdup(dir_entry->d_name));
        da_append(tasks, task);
    }

    assertmsg(closedir(dir) == 0, "ERROR: Could not close \"tasks\" folder: %s\n", strerror(errno));
    assertmsg(errno == 0, "ERROR: Could not properly read \"tasks\" folder: %s\n", strerror(errno));
}

static bool task_contains_tag(const Task task, const char *tag)
{
    const StringView tag_sv = sv_from_cstr(tag);
    da_foreach(StringView, sv, &task.tags)
    {
        if(0 == sv_cmp(*sv, tag_sv))
        {
            return true;
        }
    }
    
    return false;
}

static bool task_matches_filter(const Task task, const Ops *ops, Stack *stack)
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

static void filter_tasks(Tasks *tasks, const Ops *ops)
{
    Stack stack = {0};
    for(size_t i = tasks->count; i > 0; --i)
    {
        stack.count = 0;
        if(!task_matches_filter(tasks->items[i - 1], ops, &stack))
        {
            task_free(tasks->items[i - 1]);
            da_remove_unordered(tasks, i - 1);
        }
    }
    da_free(&stack);
}

static void dump_tasks(const Tasks *tasks, bool all_tasks, bool verbose)
{
    da_foreach(Task, task, tasks)
    { 
        if (!all_tasks && CLOSED == task->status) continue;

        switch (task->status)
        {
            case OPEN:        printf("\033[96m"); break; // blue
            case IN_PROGRESS: printf("\033[93m"); break; // yellow
            case CLOSED:      printf("\033[90m"); break; // gray
            default: assert(0 && "Unreachable");
        }

        printf("./tasks/%s/TASK.md: [STATUS: %s, PRIORITY: %ld", 
                task->id, 
                status_to_str(task->status), 
                task->prio
        );
        if (task->tags.count > 0) 
        {
            printf(", TAGS: ");
            printf(SV_FMT, SV_ARG(*task->tags.items));
            for(size_t i = 1; i < task->tags.count; ++i)
            {
                printf(", "SV_FMT, SV_ARG(task->tags.items[i]));
            } 
        }
        // TODO validate references
        if (task->refs.count > 0) 
        {
            printf(", REFERENCES: ");
            printf(SV_FMT, SV_ARG(*task->refs.items));
            for(size_t i = 1; i < task->refs.count; ++i)
            {
                printf(", "SV_FMT, SV_ARG(task->refs.items[i]));
            } 
        }
        assertmsg(task->title.len >= 2, "ERROR: malformed title\""SV_FMT"\"\n", SV_ARG(task->title));
        const StringView tmp_title = {.s = task->title.s + 2, .len = task->title.len - 2};
        printf("] "SV_FMT"\033[0m\n", SV_ARG(tmp_title));
        if (verbose)
        {
            StringView content = sv_from_sb(&task->backing);
            printf("--------------------\n"SV_FMT"\n--------------------\n\n\n", SV_ARG(content));
        }
    }
}

static void create_new_task(void)
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

    char refs[BUFSIZE];
    printf("References (optional, comma separated): ");
    assertmsg(fgets(refs, sizeof refs, stdin) != NULL, "ERROR: Could not read refs\n");
    refs[strcspn(refs, "\n")] = 0;

    create_task_folder();
    char path[BUFSIZE];
    const int offset = snprintf(path, BUFSIZE, "./tasks/%s/", timestamp);
    assert(offset >= 0);
    if(mkdir(path, 0755) != 0)
    {
        fprintf(stderr, "ERROR: Could not create \"%s\" folder: %s\n", path, strerror(errno));
        exit(1);
    }
    
    snprintf(path + offset, BUFSIZE - (size_t) offset, "TASK.md"); 
    FILE *fp;
    assertmsg((fp = fopen(path, "w")) != NULL, "ERROR: Could not open \"%s\" for writing\n", path);

    fprintf(fp, "# %s\n\n", title);
    fprintf(fp, "- STATUS: %s\n", status);
    fprintf(fp, "- PRIORITY: %ld\n", prio);
    if(strlen(tags) > 0) fprintf(fp, "- TAGS: %s\n", tags);
    if(strlen(refs) > 0) fprintf(fp, "- REFERENCES: %s\n", refs);

    fclose(fp);

    printf("INFO: Wrote \"%s\"\n", path);
}

static void print_usage(const char *program)
{
    printf("USAGE: %s <subcommand> [options]\n\n", program);
    printf("SUBCOMMANDS:\n");
    printf("    new                        Create a new task interactively\n");
    printf("    list [options]             List, sort, and filter active tasks (default)\n");
    printf("    edit <task-id> [options]   Modify an existing task's metadata\n");
    printf("    help                       Print this help menu\n\n");
    
    printf("OPTIONS FOR \"list\":\n");
    printf("    -d, --date                 Sort by creation date (descending)\n");
    printf("    -p, --priority             Sort by priority (descending, default)\n");
    printf("    -t, --topological          Sort refereces by topological ordering (descending)\n");
    printf("    -r, --reverse              Reverse the sorting order (ascending)\n");
    printf("    -f, --filter <query>       Filter tasks using a query string\n");
    printf("    -a, --all                  Display all tasks, even the closed ones\n");
    printf("    -v, --verbose              Display the whole task file\n");
    printf("                               Syntax:  '.<tag>', 'and', 'or', 'not', 'tagged', 'untagged', '(', ')'\n");
    printf("                               Example: list -f \".bug or untagged\"\n\n");
    
    printf("OPTIONS FOR \"edit\":\n");
    printf("    -s, --status <STATUS>      Set task status (OPEN, IN_PROGRESS, CLOSED)\n");
    printf("    -p, --priority <0-100>     Set task priority\n");
    printf("    -t, --tags <tags>          Overwrite task tags (comma-separated)\n");
    printf("    -r, --references <refs>    Overwrite task references (comma-separated)\n");
    printf("                               Example: edit 20260328-085723 -s IN_PROGRESS -p 80\n");
}

static SortOptions sort_options = {.desc = true, .target = SORT_PRIO};

static int task_cmp(const void *a, const void *b)
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
                return strcmp(task_b->id, task_a->id);
            }
            else
            {
                return strcmp(task_a->id, task_b->id);
            }
        } break;
        default: assertmsg(false, "UNREACHABLE");
    }
}

static void sort_topological(Tasks *tasks)
{
    typedef struct {
        size_t count;
        size_t capacity;
        size_t *items;
    } AdjList;

    if(0 == tasks->count) return;

    size_t n = tasks->count;
    HashTable id_to_idx = {0};

    for(size_t i = 0; i < n; ++i)
    {
        StringView id_sv = sv_from_cstr(tasks->items[i].id);
        assertmsg(ht_insert(&id_to_idx, id_sv, i) == 0, "ERROR: duplicate or failed task ID insert: %s\n", tasks->items[i].id);
    }

    size_t *in_edges = calloc(n, sizeof(size_t));
    AdjList *dependents = calloc(n, sizeof(AdjList));
    size_t *queue = malloc(sizeof(size_t) * n);
    Task *sorted = malloc(sizeof(Task) * n);
    assertmsg(in_edges && dependents && queue && sorted, "ERROR: out of memory\n");

    for (size_t i = 0; i < n; ++i)
    {
        in_edges[i] = tasks->items[i].refs.count;
        da_foreach(StringView, ref, &tasks->items[i].refs)
        {
            size_t target_idx = 0;
            if (ht_lookup(&id_to_idx, *ref, &target_idx) != 0)
            {
                fprintf(stderr, "ERROR: task '%s' references unknown task '"SV_FMT"'\n",
                        tasks->items[i].id, SV_ARG(*ref));
                goto err;
            }
            da_append(&dependents[target_idx], i);
        }
    }

    size_t q_head = 0, q_tail = 0;
    for (size_t i = 0; i < n; ++i)
    {
        if (in_edges[i] == 0) queue[q_tail++] = i;
    }


    size_t placed = 0;
    ssize_t write_idx = sort_options.desc ? 0 : (ssize_t)n - 1;
    const ssize_t step = sort_options.desc ? 1 : -1;

    while (q_head < q_tail)
    {
        const size_t cur_idx = queue[q_head++];
        sorted[write_idx] = tasks->items[cur_idx];
        write_idx += step;
        placed++;

        da_foreach(size_t, dep_idx, &dependents[cur_idx])
        {
            if (--in_edges[*dep_idx] == 0)
            {
                queue[q_tail++] = *dep_idx;
            }
        }
    }

    for (size_t k = 0; k < n; ++k) da_free(&dependents[k]);
    free(dependents);
    free(queue);
    free(in_edges);
    ht_free(&id_to_idx);

    tasks->items = sorted;
    return;

err:
    for (size_t k = 0; k < n; ++k) da_free(&dependents[k]);
    free(dependents);
    free(sorted);
    free(queue);
    free(in_edges);
    ht_free(&id_to_idx);
    exit(1);
}

static int list_tasks(int argc, char **argv)
{
    Tasks tasks = {0};
    load_tasks(&tasks);
    bool all_tasks = false;
    bool verbose = false;

    // TODO help flag for list
    // TODO maybe convert this to getopt, to allow for something like -dr
    while(argc > 0)
    {
        const char *flag = shift_args(argc, argv);
        if(0 == strcmp(flag, "-f") || 0 == strcmp(flag, "--filter"))
        {
            const char *filter = shift_args(argc, argv);
            assertmsg(argc >= 0, "ERROR: no filter provided!\n");
            Lexer *lexer = lexer_new(filter);
            if (!lexer) goto defer;

            Ops ops = {0};
            parse_expr(lexer, &ops);
            filter_tasks(&tasks, &ops);

            da_foreach(Op, op, &ops)
            {
                free(op->lexeme);
            }
            da_free(&ops);
            lexer_free(lexer);
        }
        else if(0 == strcmp("-d", flag) || 0 == strcmp("--date", flag))
        {
            sort_options.target = SORT_TIMESTAMP;
        }
        else if(0 == strcmp("-p", flag) || 0 == strcmp("--priority", flag))
        {
            sort_options.target = SORT_PRIO;
        }
        else if(0 == strcmp("-t", flag) || 0 == strcmp("--topological", flag))
        {
            sort_options.target = SORT_TOPOLOGICAL;
        }
        else if(0 == strcmp("-r", flag) || 0 == strcmp("--reverse", flag))
        {
            sort_options.desc = false;
        }
        else if(0 == strcmp("-a", flag) || 0 == strcmp("--all", flag))
        {
            all_tasks = true;
        }
        else if(0 == strcmp("-v", flag) || 0 == strcmp("--verbose", flag))
        {
            verbose = true;
        }
        else
        {
            fprintf(stderr, "ERROR: unknown list flag '%s'\n", flag);
            return 1;
        }
    }

    if(sort_options.target != SORT_TOPOLOGICAL)
    {
        da_sort(&tasks, task_cmp);
    }
    else
    {
        sort_topological(&tasks);
    }
    dump_tasks(&tasks, all_tasks, verbose);

defer:
    da_foreach(Task, task, &tasks)
    {
        task_free(*task);
    }
    da_free(&tasks);

    return 0;
}

static void append_missing(StringBuilder *out, 
        const char *tags, bool has_tags,
        const char *refs, bool has_refs)
{
    char buf[BUFSIZE];
    if(tags && *tags && !has_tags)
    {
        const int len = snprintf(buf, sizeof buf, "- TAGS: %s\n", tags);
        assert(len >= 0);
        da_append_many(out, buf, (size_t) len);
    }
    if(refs && *refs && !has_refs)
    {
        const int len = snprintf(buf, sizeof buf, "- REFERENCES: %s\n", refs);
        assert(len >= 0);
        da_append_many(out, buf, (size_t) len);
    }
}

static int edit_tasks(int argc, char **argv)
{
    if (argc < 1) 
    {
        fprintf(stderr, "USAGE: tt edit <task-id> [options]\n");
        fprintf(stderr, "OPTIONS:\n");
        fprintf(stderr, "    -s, --status <STATUS>           Set task status (OPEN, IN_PROGRESS, CLOSED)\n");
        fprintf(stderr, "    -p, --priority <0-100>          Set task priority\n");
        fprintf(stderr, "    -t, --tags <tags>               Overwrite task tags (comma-separated)\n");
        fprintf(stderr, "    -r, --references <references>   Overwrite task references (comma-separated)\n");
        return 1;
    }

    const char *task_id = shift_args(argc, argv);

    const char *new_status = NULL;
    const char *new_tags   = NULL;
    const char *new_refs   = NULL;
    long new_prio          = -1;

    while(argc > 0)
    {
        const char *flag = shift_args(argc, argv);
        if(0 == strcmp(flag, "-s") || 0 == strcmp(flag, "--status"))
        {
            assertmsg(argc > 0, "ERROR: status flag required an argument\n");
            new_status = shift_args(argc, argv);
            static_assert(status_count_ == 3, "status count has changed");
            if(0 != strcmp(new_status, "OPEN") &&
                    0 != strcmp(new_status, "IN_PROGRESS") &&
                    0 != strcmp(new_status, "CLOSED"))
            {
                fprintf(stderr, "ERROR: Invalid status \"%s\"\n", new_status);
                fprintf(stderr, "ERROR: Available options: \"OPEN\", \"IN_PROGRESS\", \"CLOSED\"\n");
                return 1; 
            }

        }
        else if(0 == strcmp(flag, "-p") || 0 == strcmp(flag, "--priority"))
        {
            assertmsg(argc > 0, "ERROR: priority flag required an argument\n");
            const char *prio_str = shift_args(argc, argv);
            char *endptr;
            new_prio = strtol(prio_str, &endptr, 10);
            assertmsg(*endptr == 0 && new_prio >= 0 && new_prio <= 100, "ERROR: Invalid priority (0-100)\n");
        }
        else if(0 == strcmp(flag, "-t") || 0 == strcmp(flag, "--tags"))
        {
            assertmsg(argc > 0, "ERROR: tags flag required an argument");
            new_tags = shift_args(argc, argv);
        }
        else if(0 == strcmp(flag, "-r") || 0 == strcmp(flag, "--references"))
        {
            assertmsg(argc > 0, "ERROR: references flag required an argument");
            new_refs = shift_args(argc, argv);
        }
        else
        {
            fprintf(stderr, "ERROR: Unknown edit option '%s'\n", flag);
            return 1;
        }
    }

    if(!new_status && new_prio == -1 && !new_tags && !new_refs)
    {
        fprintf(stderr, "WARNING: Nothing to edit. Provide -s, -p, -t, or -r flags.\n");
        return 0;
    }

    char path[BUFSIZE];
    snprintf(path, sizeof(path), "./tasks/%s/TASK.md", task_id);

    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    StringBuilder output = {0};
    StringView line;
    bool has_tags = false;
    bool has_refs = false;
    bool preamble_passed = false;
    size_t line_no = 0;

    while((line = sv_chop(&sv, '\n')).len > 0 || sv.len > 0)
    {
        line_no++;

        if(preamble_passed || line_no <= 2)
        {
            da_append_many(&output, line.s, line.len);
            da_append(&output, '\n');
            continue;
        }

        if(0 == line.len)
        {
            preamble_passed = true;
            append_missing(&output, new_tags, has_tags, new_refs, has_refs);
            da_append(&output, '\n');
        }
        else if(sv_is_prefix(line, "- STATUS") && new_status)
        {
            char buf[128];
            const int len = snprintf(buf, sizeof(buf), "- STATUS: %s\n", new_status);
            assert(len >= 0);
            da_append_many(&output, buf, (size_t) len);
        }
        else if(sv_is_prefix(line, "- PRIORITY") && new_prio != -1)
        {
            char buf[128];
            const int len = snprintf(buf, sizeof(buf), "- PRIORITY: %ld\n", new_prio);
            assert(len >= 0);
            da_append_many(&output, buf, (size_t) len);
        }
        else if(sv_is_prefix(line, "- TAGS"))
        {
            has_tags = true;
            if(!new_tags)
            {
                da_append_many(&output, line.s, line.len);
                da_append(&output, '\n');
            }
            else if(*new_tags)
            {
                char buf[BUFSIZE];
                const int len = snprintf(buf, sizeof(buf), "- TAGS: %s\n", new_tags);
                assert(len >= 0);
                da_append_many(&output, buf, (size_t) len);
            }
        }
        else if(sv_is_prefix(line, "- REFERENCES"))
        {
            has_refs = true;
            if(!new_refs)
            {
                da_append_many(&output, line.s, line.len);
                da_append(&output, '\n');
            }
            else if(*new_refs)
            {
                char buf[BUFSIZE];
                const int len = snprintf(buf, sizeof(buf), "- REFERENCES: %s\n", new_refs);
                assert(len >= 0);
                da_append_many(&output, buf, (size_t) len);
            }
        }
        else
        {
            da_append_many(&output, line.s, line.len);
            da_append(&output, '\n');
        }
    }

    if(!preamble_passed) append_missing(&output, new_tags, has_tags, new_refs, has_refs);

    FILE *fp = fopen(path, "w");
    assertmsg(fp != NULL, "ERROR: Could not write to file\n");
    fwrite(output.items, 1, output.count, fp);
    fclose(fp);

    printf("SUCCESS: Updated task %s\n", task_id);

    da_free(&sb);
    da_free(&output);

    return 0;
}

int main(int argc, char **argv)
{
    const char *program = shift_args(argc, argv);
    if(0 == argc)
    {
        list_tasks(argc, argv);
        return 0;
    }

    const char *subcommand = shift_args(argc, argv);

    if(0 == strcmp(subcommand, "new"))
    {
        create_new_task();
    }
    else if(0 == strcmp(subcommand, "list"))
    {
        list_tasks(argc, argv); 
    }
    else if(0 == strcmp(subcommand, "edit"))
    {
        edit_tasks(argc, argv);
    }
    else if(0 == strcmp(subcommand, "help"))
    {
        print_usage(program);
    }
    else
    {
        fprintf(stderr, "ERROR: Unknown subcommand or flag: '%s'\n", subcommand);
        print_usage(program);
        return 1;
    }

    return 0;
}
