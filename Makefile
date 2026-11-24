CC := gcc
CFLAGS := -Wall -Wextra -Werror -Wconversion -Wformat=2 -pedantic -std=c17 -O3
CPPFLAGS := -MMD -MP

DESTDIR  := build
TARGET   := tt

SRC      := src/main.c src/filter.c src/common.c src/task.c
HDR_MODS := da ht arena flag
C_OBJS   := $(patsubst src/%.c,$(DESTDIR)/%.o,$(SRC))
H_OBJS   := $(patsubst %,$(DESTDIR)/%.o,$(HDR_MODS))
OBJS     := $(H_OBJS) $(C_OBJS)
DEPS     := $(OBJS:.o=.d)

.PHONY: all install clean test fmt tidy

all: $(TARGET) test

$(DESTDIR):
	mkdir -p $@

$(DESTDIR)/da.o: src/da.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DDA_IMPLEMENTATION -o $@ $<

$(DESTDIR)/ht.o: src/ht.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DHT_IMPLEMENTATION -o $@ $< 

$(DESTDIR)/arena.o: src/arena.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DARENA_IMPLEMENTATION -o $@ $<

$(DESTDIR)/flag.o: src/flag.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DFLAG_IMPLEMENTATION -o $@ $<

$(DESTDIR)/%.o: src/%.c | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEPS)

test: $(TARGET)
	pytest ./tests

fmt:
	git ls-files '*.c' '*.h' | xargs clang-format -i

tidy:
	clang-tidy --quiet $(SRC) -- $(CPPFLAGS) $(CFLAGS)

install:
	mkdir -p $(DESTDIR)/bin
	cp tt $(DESTDIR)/bin/tt

clean:
	rm -f tt $(TEST_BIN)

