CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O2

tt: src/main.c src/parser.c
	$(CC) $(CFLAGS) $^ -o $@
