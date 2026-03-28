CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O3

tt: main.c parser.c
	$(CC) $(CFLAGS) $^ -o $@
