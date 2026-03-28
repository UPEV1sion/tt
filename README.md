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
        syntax: '.<tag>', 'and', 'or', 'not', 'tagged', untagged, '(' and ')'
        example: -f ".bug or untagged"
        example: -f ".unfinished and not (.feature or .refactor)"
``` 
