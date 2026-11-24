#ifndef TASK_H_
#define TASK_H_

#include "da.h"

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

typedef enum {
    SORT_TIMESTAMP,
    SORT_PRIO,
    SORT_TOPOLOGICAL,
} SortTarget;

typedef enum {
    SORT_ORDER_DESC,
    SORT_ORDER_ASC,
} SortOrder;

typedef struct {
    SortTarget target;
    SortOrder order;
} SortOptions;

void load_tasks(Tasks *tasks);
void dump_tasks(const Tasks *tasks, bool all_tasks, bool verbose);
void create_new_task(void);
void sort_tasks(Tasks *tasks, SortOptions sort_options);
bool task_contains_tag(Task task, const char *tag);
void free_task(Task tasks);
void free_tasks(Tasks *tasks);

#endif // TASK_H_
