//
// Created by escha on 26.03.26.
//

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <inttypes.h>

#include "common.h"
#include "da.h"
#include "filter.h"
#include "flag.h"
#include "task.h"

#define shift_args(argc, argv) ((argc)--, *(argv)++)

static void print_usage(const char *program)
{
    // TODO(escha): how can this be synced with flag decls?
    printf("USAGE: %s <subcommand> [options]\n\n", program);
    printf("SUBCOMMANDS:\n");
    printf("    new                      Create a new task interactively\n");
    printf("    list [flags]             List, sort, and filter active tasks (default)\n");
    printf("    edit <task-id> [flags]   Modify an existing task's metadata\n");
    printf("    help                     Print this help menu\n\n");

    printf("OPTIONS FOR \"list\":\n");
    printf("    -d, --date                 Sort by creation date (descending)\n");
    printf("    -p, --priority             Sort by priority (descending, default)\n");
    printf("    -g, --graph                Sort references by topological ordering (descending)\n");
    printf("    -i, --inverse              Inverse the sorting order (ascending)\n");
    printf("    -f, --filter <query>       Filter tasks using a query string\n");
    printf("                               Syntax:  '.<tag>', 'and', 'or', 'not', 'tagged', 'untagged', '(', ')'\n");
    printf("                               Example: list -f \".bug or untagged\"\n");
    printf("    -a, --all                  Display all tasks, even the closed ones\n");
    printf("    -v, --verbose              Display the whole task file\n");
    printf("    -h, --help                 Display the help menu for the list subcommand\n\n");

    printf("OPTIONS FOR \"edit\":\n");
    printf("    -s, --status <STATUS>      Set task status (OPEN, IN_PROGRESS, CLOSED)\n");
    printf("    -p, --priority <0-100>     Set task priority\n");
    printf("    -t, --tags <tags>          Overwrite task tags comma-separated)\n");
    printf("    -r, --references <refs>    Overwrite task references (comma-separated)\n");
    printf("                               Example: edit 20260328-085723 -s IN_PROGRESS -p 80\n");
    printf("    -h, --help                 Display the help menu for the edit subcommand\n\n");
}

static int list_tasks(int argc, char **argv)
{
    // clang-format off
    Flags flags = {0};
    char *filter;
    bool date_sort, prio_sort, topo_sort, reverse, all_tasks, verbose, help;
    flag_decl_str(&flags,  &filter,    "-f", "--filter",   NULL,  "Filter tasks using the query language");
    flag_decl_bool(&flags, &date_sort, "-d", "--date",     false, "Sort by creation date (descending)");
    flag_decl_bool(&flags, &prio_sort, "-p", "--priority", true,  "Sort by priority (descending, default)");
    flag_decl_bool(&flags, &topo_sort, "-g", "--graph",    false, "Sort references by topological ordering (descending)");
    flag_decl_bool(&flags, &reverse,   "-i", "--inverse",  false, "Inverse the sorting order (ascending)");
    flag_decl_bool(&flags, &all_tasks, "-a", "--all",      false, "Display all tasks, even the closed ones");
    flag_decl_bool(&flags, &verbose,   "-v", "--verbose",  false, "Display the whole task file");
    flag_decl_bool(&flags, &help,      "-h", "--help",     false, "Display help for the list subcommand");
    assert(flags.count == 8 && "flags changed, remember to change README and print_usage!");
    if (flag_parse(&flags, argc, argv) != 0)
    {
        flag_print_info(&flags, "LIST FLAGS");
        flag_free(&flags);
        return 1;
    }
    if (help)
    {
        flag_print_info(&flags, "LIST FLAGS");
        flag_free(&flags);
        return 0;
    }
    flag_free(&flags);
    // clang-format on
    
    Ops ops = {0};
    if (filter)
    {
        Lexer *lexer = lexer_new(filter);
        if (!lexer) return 1;
        parse_expr(lexer, &ops);
        lexer_free(lexer);
    }

    SortOptions sort_options;
    sort_options.order       = reverse ? SORT_ORDER_ASC : SORT_ORDER_DESC;
    if (prio_sort) sort_options.target = SORT_PRIO;
    if (date_sort) sort_options.target = SORT_TIMESTAMP;
    if (topo_sort) sort_options.target = SORT_TOPOLOGICAL;

    Tasks tasks = {0};
    load_tasks(&tasks);
    sort_tasks(&tasks, sort_options);
    if(filter) filter_tasks(&tasks, &ops);
    dump_tasks(&tasks, all_tasks, verbose);

    op_free(&ops);
    free_tasks(&tasks);

    return 0;
}

static void append_missing(StringBuilder *out, const char *tags, bool has_tags, const char *refs, bool has_refs)
{
    if (tags && *tags && !has_tags) sb_appendf(out, "- TAGS: %s\n", tags);
    if (refs && *refs && !has_refs) sb_appendf(out, "- REFERENCES: %s\n", refs);
}

static int edit_task(int argc, char **argv)
{
    const char *task_id = shift_args(argc, argv);

    char *new_status = NULL;
    char *new_tags   = NULL;
    char *new_refs   = NULL;
    int64_t new_prio = -1;
    bool help        = false;

    // clang-format off
    Flags flags = {0};
    flag_decl_str(&flags,  &new_status, "-s", "--status",     NULL,  "Set task status (OPEN, IN_PROGRESS, CLOSED)");
    flag_decl_int(&flags,  &new_prio,   "-p", "--priority",   -1,    "Set task priority (0-100)");
    flag_decl_str(&flags,  &new_tags,   "-t", "--tags",       NULL,  "Override tasks tags (comma-separated)");
    flag_decl_str(&flags,  &new_refs,   "-r", "--references", NULL,  "Override tasks references (comma-separated)");
    flag_decl_bool(&flags, &help,       "-h", "--help",       false, "Display help for the edit subcommand");
    assert(flags.count == 5 && "flags changed, remember to change README and print_usage!");
    if (flag_parse(&flags, argc, argv) != 0)
    {
        flag_print_info(&flags, "EDIT FLAGS");
        flag_free(&flags);
        return 1;
    }
    bool prio_set = flag_seen(&flags, "-p", "--priority");
    if (help || argc <= 0)
    {
        flag_print_info(&flags, "EDIT FLAGS");
        flag_free(&flags);
        return 0;
    }
    // clang-format on

    // TODO err/defer goto
    if (new_status)
    {
        static_assert(status_count_ == 3, "status count has changed");
        if (0 != strcmp(new_status, "OPEN") && 0 != strcmp(new_status, "IN_PROGRESS") &&
            0 != strcmp(new_status, "CLOSED"))
        {
            fprintf(stderr, "ERROR: Invalid status \"%s\"\n", new_status);
            fprintf(stderr, "ERROR: Available options: \"OPEN\", \"IN_PROGRESS\", \"CLOSED\"\n");
            flag_free(&flags);
            return 1;
        }
    }

    if (prio_set && (new_prio < 0 || new_prio > 100))
    {
        fprintf(stderr, "ERROR: Invalid priority %" PRId64 " (allowed range: 0-100)\n", new_prio);
        flag_free(&flags);
        return 1;
    }

    if (!new_status && !prio_set && !new_tags && !new_refs)
    {
        fprintf(stderr, "WARNING: Nothing to edit.\n");
        flag_print_info(&flags, "EDIT FLAGS");
        flag_free(&flags);
        return 0;
    }

    flag_free(&flags);

    char path[BUFSIZE];
    char tmp_path[BUFSIZE];
    snprintf(path, sizeof(path), "./tasks/%s/TASK.md", task_id);
    snprintf(tmp_path, sizeof(tmp_path), "./tasks/%s/TASK.md.tmp", task_id);

    StringBuilder sb = {0};
    read_file(&sb, path);
    StringView sv = sv_from_sb(&sb);

    StringBuilder output = {0};
    StringView line;
    bool has_tags        = false;
    bool has_refs        = false;
    bool preamble_passed = false;
    size_t line_no       = 0;

    while ((line = sv_chop(&sv, '\n')).len > 0 || sv.len > 0)
    {
        line_no++;

        if (preamble_passed || line_no <= 2)
        {
            da_append_many(&output, line.s, line.len);
            da_append(&output, '\n');
            continue;
        }

        if (0 == line.len)
        {
            preamble_passed = true;
            append_missing(&output, new_tags, has_tags, new_refs, has_refs);
            da_append(&output, '\n');
        }
        else if (sv_is_prefix(line, "- STATUS") && new_status)
        {
            sb_appendf(&output, "- STATUS: %s\n", new_status);
        }
        else if (sv_is_prefix(line, "- PRIORITY") && prio_set)
        {
            sb_appendf(&output, "- PRIORITY: %" PRId64 "\n", new_prio);
        }
        else if (sv_is_prefix(line, "- TAGS"))
        {
            has_tags = true;
            if (!new_tags)
            {
                da_append_many(&output, line.s, line.len);
                da_append(&output, '\n');
            }
            else if (*new_tags)
            {
                sb_appendf(&output, "- TAGS: %s\n", new_tags);
            }
        }
        else if (sv_is_prefix(line, "- REFERENCES"))
        {
            has_refs = true;
            if (!new_refs)
            {
                da_append_many(&output, line.s, line.len);
                da_append(&output, '\n');
            }
            else if (*new_refs)
            {
                sb_appendf(&output, "- REFERENCES: %s\n", new_refs);
            }
        }
        else
        {
            da_append_many(&output, line.s, line.len);
            da_append(&output, '\n');
        }
    }

    if (!preamble_passed) append_missing(&output, new_tags, has_tags, new_refs, has_refs);

    FILE *fp = fopen(tmp_path, "wbe");
    if (!fp)
    {
        fprintf(stderr, "ERROR: Could not create temp file \"%s\": %s\n", tmp_path, strerror(errno));
        return 1;
    }

    if (fwrite(output.items, 1, output.count, fp) != output.count)
    {
        fprintf(stderr, "ERROR: Could not write to temp file \"%s\": %s\n", tmp_path, strerror(errno));
        fclose(fp);
        unlink(tmp_path);
        return 1;
    }

    if (fclose(fp) != 0)
    {
        fprintf(stderr, "ERROR: Could not close temp file \"%s\": %s\n", tmp_path, strerror(errno));
        unlink(tmp_path);
        return 1;
    }

    if (rename(tmp_path, path) != 0)
    {
        fprintf(stderr, "ERROR: Could not replace \"%s\" with \"%s\": %s\n", path, tmp_path, strerror(errno));
        unlink(tmp_path);
        return 1;
    }

    printf("SUCCESS: Updated task %s\n", task_id);

    da_free(&sb);
    da_free(&output);

    return 0;
}

int main(int argc, char **argv)
{
    const char *program = shift_args(argc, argv);
    if (0 == argc)
    {
        return list_tasks(argc, argv);
    }

    const char *subcommand = shift_args(argc, argv);

    if (0 == strcmp(subcommand, "new"))
    {
        create_new_task();
    }
    else if (0 == strcmp(subcommand, "list"))
    {
        return list_tasks(argc, argv);
    }
    else if (0 == strcmp(subcommand, "edit"))
    {
        return edit_task(argc, argv);
    }
    else if (0 == strcmp(subcommand, "help"))
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
