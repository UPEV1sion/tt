CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O2

tt: main.c parser.c
	$(CC) $(CFLAGS) $^ -o $@
