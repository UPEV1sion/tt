#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "da.h"
#include "ht.h"
#include "task.h"

static const char *status_to_str(const TaskStatus status)
{
    switch (status)
    {
    case OPEN:        return "OPEN";
    case IN_PROGRESS: return "IN_PROGRESS";
    case CLOSED:      return "CLOSED";
    default:          assert(0 && "Unreacheable");
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

static void create_task_folder(void)
{
    struct stat st;
    if (stat("./tasks", &st) == 0 && S_ISDIR(st.st_mode)) return;

    fprintf(stderr, "ERROR: No \"tasks\" folder found!\n");
    fprintf(stderr, "INFO: Creating new \"tasks\" folder\n");
    if (mkdir("./tasks", 0755) != 0)
    {
        fprintf(stderr, "ERROR: Could not create \"tasks\" folder: %s", strerror(errno));
        exit(1);
    }
}

void free_task(Task task)
{
    free(task.id);
    da_free(&task.backing);
    da_free(&task.tags);
    da_free(&task.refs);
}

bool task_contains_tag(const Task task, const char *tag)
{
    const StringView tag_sv = sv_from_cstr(tag);
    da_foreach(StringView, sv, &task.tags)
    {
        if (0 == sv_cmp(*sv, tag_sv))
        {
            return true;
        }
    }

    return false;
}

void dump_tasks(const Tasks *tasks, bool all_tasks, bool verbose)
{
    const bool color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    da_foreach(Task, task, tasks)
    {
        if (!all_tasks && CLOSED == task->status) continue;

        if (color)
        {
            switch (task->status)
            {
            case OPEN:        printf("\033[96m"); break; // blue
            case IN_PROGRESS: printf("\033[93m"); break; // yellow
            case CLOSED:      printf("\033[90m"); break; // gray
            default:          assert(0 && "Unreachable");
            }
        }

        printf("./tasks/%s/TASK.md: [STATUS: %s, PRIORITY: %ld", task->id, status_to_str(task->status), task->prio);
        if (task->tags.count > 0)
        {
            printf(", TAGS: ");
            printf(SV_FMT, SV_ARG(*task->tags.items));
            for (size_t i = 1; i < task->tags.count; ++i)
            {
                printf(", " SV_FMT, SV_ARG(task->tags.items[i]));
            }
        }
        if (task->refs.count > 0)
        {
            printf(", REFERENCES: ");
            printf(SV_FMT, SV_ARG(*task->refs.items));
            for (size_t i = 1; i < task->refs.count; ++i)
            {
                printf(", " SV_FMT, SV_ARG(task->refs.items[i]));
            }
        }
        const StringView tmp_title = task->title.len > 2
                                         ? (StringView){.s = task->title.s + 2, .len = task->title.len - 2}
                                         : sv_from_cstr("(untitled");
        printf("] " SV_FMT "\n", SV_ARG(tmp_title));
        if (color) printf("\033[0m");
        if (verbose)
        {
            StringView content = sv_from_sb(&task->backing);
            printf("--------------------\n" SV_FMT "\n--------------------\n\n\n", SV_ARG(content));
        }
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
    char *states[]                = {"OPEN", "IN_PROGRESS", "CLOSED"};
    bool valid_state              = false;
    for (size_t i = 0; i < ARRAY_LEN(states); ++i)
    {
        valid_state = valid_state || (0 == strcmp(states[i], status));
    }
    if (!valid_state)
    {
        fprintf(stderr, "ERROR: invalid status \"%s\"\n", status);
        fprintf(stderr,
                "ERROR: Available options: \"OPEN\", \"IN_PROGRESS\", "
                "\"CLOSED\"\n");
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
    if (mkdir(path, 0755) != 0)
    {
        fprintf(stderr, "ERROR: Could not create \"%s\" folder: %s\n", path, strerror(errno));
        exit(1);
    }

    snprintf(path + offset, BUFSIZE - (size_t)offset, "TASK.md");
    FILE *fp = NULL;
    assertmsg((fp = fopen(path, "we")) != NULL, "ERROR: Could not open \"%s\" for writing\n", path);

    fprintf(fp, "# %s\n\n", title);
    fprintf(fp, "- STATUS: %s\n", status);
    fprintf(fp, "- PRIORITY: %ld\n", prio);
    if (strlen(tags) > 0) fprintf(fp, "- TAGS: %s\n", tags);
    if (strlen(refs) > 0) fprintf(fp, "- REFERENCES: %s\n", refs);

    fclose(fp);

    printf("INFO: Wrote \"%s\"\n", path);
}

static Task parse_task(char *id)
{
    char path[BUFSIZE];
    snprintf(path, BUFSIZE, "./tasks/%s/TASK.md", id);

    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    const StringView title = sv_chop(&sv, '\n');
    Task task              = {
                     .title   = title,
                     .id      = id,
                     .backing = sb,
    };
    sv_chop(&sv, '\n');

    StringView meta;
    while ((meta = sv_chop(&sv, '\n')).len > 0)
    {
        if (sv_is_prefix(meta, "- PRIORITY"))
        {
            sv_chop(&meta, ':');
            const StringView value = sv_trim(meta);

            char prio_buf[32];
            if (value.len == 0 || value.len >= sizeof prio_buf)
            {
                fprintf(stderr, "ERROR: Malformed \"PRIORITY\" header in %s\n", path);
                exit(1);
            }
            memcpy(prio_buf, value.s, value.len);
            prio_buf[value.len] = 0;

            char *endptr   = NULL;
            errno          = 0;
            const long num = strtol(prio_buf, &endptr, 10);
            if (errno != 0 || *endptr != 0 || num < 0 || num > 100)
            {
                fprintf(stderr, "ERROR: Invalid priority \"%s\" in %s (expected 0-100)\n", prio_buf, path);
                exit(1);
            }
            task.prio = num;
        }
        else if (sv_is_prefix(meta, "- STATUS"))
        {
            sv_chop(&meta, ':');
            const StringView trimmed = sv_trim(meta);
            if (trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"STATUS\" header in %s\n", path);
                exit(1);
            }

            static_assert(status_count_ == 3, "ERROR: non exhaustive handling of STATUSES");
            if (sv_equal(trimmed, sv_from_cstr("OPEN")))
            {
                task.status = OPEN;
            }
            else if (sv_equal(trimmed, sv_from_cstr("IN_PROGRESS")))
            {
                task.status = IN_PROGRESS;
            }
            else if (sv_equal(trimmed, sv_from_cstr("CLOSED")))
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
        else if (sv_is_prefix(meta, "- TAGS"))
        {
            sv_chop(&meta, ':');
            StringView trimmed = sv_trim(meta);
            if (trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"TAGS\" header in %s\n", path);
                exit(1);
            }

            while (trimmed.len > 0)
            {
                const StringView tag = sv_trim(sv_chop(&trimmed, ','));
                if (tag.len > 0) da_append(&task.tags, tag);
            }
        }
        else if (sv_is_prefix(meta, "- REFERENCES"))
        {
            sv_chop(&meta, ':');
            StringView trimmed = sv_trim(meta);
            if (trimmed.len == 0)
            {
                fprintf(stderr, "ERROR: Malformed \"REFERENCES\" header in %s\n", path);
                exit(1);
            }

            while (trimmed.len > 0)
            {
                const StringView tag = sv_trim(sv_chop(&trimmed, ','));
                if (tag.len > 0) da_append(&task.refs, tag);
            }
        }
        else if (*meta.s == '-')
        {
            fprintf(stderr, "ERROR: Invalid task header \"" SV_FMT "\" in %s\n", SV_ARG(meta), path);
            exit(1);
        }
        else if (*meta.s == '\n')
        {
            break;
        }
    }

    return task;
}

void load_tasks(Tasks *tasks)
{
    DIR *dir = opendir("./tasks");
    if (!dir)
    {
        fprintf(stderr, "ERROR: No \"tasks\" folder found!\n");
        return;
    }
    errno                    = 0;
    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)) != NULL)
    {
        if (0 == strcmp(dir_entry->d_name, ".") || 0 == strcmp(dir_entry->d_name, "..")) continue;

        char path[BUFSIZE];
        snprintf(path, sizeof(path), "./tasks/%s", dir_entry->d_name);
        struct stat sb;
        if (stat(path, &sb) != 0)
        {
            fprintf(stderr, "ERROR: could not stat %s: %s. Continuing...\n", path, strerror(errno));
            continue;
        }

        if (!S_ISDIR(sb.st_mode))
        {
            fprintf(stderr,
                    "ERROR: malformed \"tasks\" folder. Please remove \"./tasks/%s\"! Continuing...\n",
                    dir_entry->d_name);
            continue;
        }
        const Task task = parse_task(strdup(dir_entry->d_name));
        da_append(tasks, task);
    }

    assertmsg(closedir(dir) == 0, "ERROR: Could not close \"tasks\" folder: %s\n", strerror(errno));
    assertmsg(errno == 0, "ERROR: Could not properly read \"tasks\" folder: %s\n", strerror(errno));
}

static void sort_topological(Tasks *tasks)
{
    size_t n = tasks->count;
    if (0 == n) return;

    typedef struct {
        size_t count;
        size_t capacity;
        size_t *items;
    } AdjList;

    HashTable id_to_idx = {0};

    for (size_t i = 0; i < n; ++i)
    {
        StringView id_sv = sv_from_cstr(tasks->items[i].id);
        assertmsg(ht_insert(&id_to_idx, id_sv, i) == 0,
                  "ERROR: duplicate or failed task ID insert: %s\n",
                  tasks->items[i].id);
    }

    size_t *in_edges    = calloc(n, sizeof(size_t));
    AdjList *dependents = calloc(n, sizeof(AdjList));
    size_t *queue       = malloc(sizeof(size_t) * n);
    Task *sorted        = malloc(sizeof(Task) * n);
    assertmsg(in_edges && dependents && queue && sorted, "ERROR: out of memory\n");

    for (size_t i = 0; i < n; ++i)
    {
        da_foreach(StringView, ref, &tasks->items[i].refs)
        {
            size_t target_idx = 0;
            if (ht_lookup(&id_to_idx, *ref, &target_idx) != 0)
            {
                fprintf(stderr,
                        "WARNING: task '%s' references unknown task '" SV_FMT "', ignoring\n",
                        tasks->items[i].id,
                        SV_ARG(*ref));
                continue;
            }
            in_edges[i]++;
            da_append(&dependents[target_idx], i);
        }
    }

    size_t q_head = 0;
    size_t q_tail = 0;
    for (size_t i = 0; i < n; ++i)
    {
        if (0 == in_edges[i]) queue[q_tail++] = i;
    }

    size_t placed    = 0;
    size_t write_idx = 0;

    while (q_head < q_tail)
    {
        const size_t cur_idx = queue[q_head++];
        sorted[write_idx]    = tasks->items[cur_idx];
        write_idx++;
        placed++;

        da_foreach(size_t, dep_idx, &dependents[cur_idx])
        {
            if (--in_edges[*dep_idx] == 0)
            {
                queue[q_tail++] = *dep_idx;
            }
        }
    }

    if (placed != n)
    {
        fprintf(stderr, "ERROR: reference cycle detected:\n");
        for (size_t i = 0; i < n; ++i)
        {
            if (in_edges[i] > 0) fprintf(stderr, "    %s\n", tasks->items[i].id);
        }
        goto err;
    }

    for (size_t k = 0; k < n; ++k) da_free(&dependents[k]);

    free(dependents);
    free(queue);
    free(in_edges);
    ht_free(&id_to_idx);
    da_free(tasks);

    tasks->items    = sorted;
    tasks->capacity = n;
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

static int prio_cmp(const void *a, const void *b)
{
    const Task *task_a = (const Task *)a;
    const Task *task_b = (const Task *)b;
    return (task_b->prio > task_a->prio) - (task_b->prio < task_a->prio);
}

static int timestamp_cmp(const void *a, const void *b)
{
    const Task *task_a = (const Task *)a;
    const Task *task_b = (const Task *)b;
    return strcmp(task_b->id, task_a->id);
}

void sort_tasks(Tasks *tasks, const SortOptions sort_options)
{
    switch (sort_options.target)
    {
    case SORT_TIMESTAMP:   da_sort(tasks, timestamp_cmp); break;
    case SORT_PRIO:        da_sort(tasks, prio_cmp); break;
    case SORT_TOPOLOGICAL: sort_topological(tasks); break;
    }

    if (SORT_ORDER_ASC == sort_options.order)
    {
        da_reverse(tasks);
    }
}

void free_tasks(Tasks *tasks)
{
    da_foreach(Task, task, tasks)
    {
        free_task(*task);
    }
    da_free(tasks);
}
