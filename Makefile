CC := gcc
CFLAGS := -Wall -Wextra -pedantic -ggdb

tt: src/main.c src/parser.c
	$(CC) $(CFLAGS) $^ -o $@
