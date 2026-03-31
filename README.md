# Task Tracker

A lightweight, git-agnostic command-line task tracker.
Create, sort, and filter tasks using simple flags and a custom query language, all within your repository.
This work is inspired by @[tsoding](https://github.com/tsoding)'s tasks tracker.

## Quick Start

```console
$ make
$ ./tt <flags>
```

## Features

This utility provides facilities to create, sort tasks and filter tasks:
```console
    -h, --help:      print help

    -n, --new:       create a new task

    -d, --date:      sort by creation date (descending)
    -D, --Date:      sort by creation date (ascending)

    -p, --priority:  sort by priority (descending, default)
    -P, --Priority:  sort by priority (ascending)

    -f, --filter:    filter existing tasks
``` 

### Filter syntax

You can filter tasks using a simple query language:
- Tags must be prefixed with a `.` (e.g. `.bug`)
- Supported operators: `and`, `or`, `not`, `(` and `)`
- Keywords:
    - `tagged` $\to$ tasks with at least on tag
    - `untagged` $\to$ tasks with no tags

Examples:
```console
./tt -f ".bug or untagged"
./tt -f ".unfinished and not (.feature or .refactor)"
```

### Task Attributes

When creating a new task, you can define the following attributes:

_STATUS_

One of:
- `OPEN`
- `IN_PROGRESS`
- `CLOSED`

By default, tasks with status `CLOSED` are excluded from the tasks list.

_PRIORITY_

- Integer value from 0 to 100

Tasks are sorted by priority (descending) by default.

_TAGS_

- Free-form values defined by the user
- Must be prefixed with `.`with filtering
- Used with the `-f` option for querying tasks


After that a markdown file will be created in a folder with a timestamp (e.g. `tasks/20260328-085723/TASK.md`) with the exemplary preamble:
```markdown
# Verbose output

- STATUS: OPEN
- PRIORITY: 20
- TAGS: feature, cli
```

The user can add arbitrary files into the created folder and write additional information _after_ the preamble.
For example:
```markdown
# Verbose output

- STATUS: OPEN
- PRIORITY: 20
- TAGS: feature, cli

Add support for verbose output (-v, --verbose), to print the content of each task.

```console
$ ./tt -v
./tasks/20260328-085723/TASK.md: [PRIORITY: 20, TAGS: feature, cli] Verbose output
> # Verbose output
> 
> - STATUS: OPEN
> - PRIORITY: 20
> - TAGS: feature, cli
> 
> Add support for verbose output (-v, --verbose), to print the content of each task.
```
```





