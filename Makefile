CC       := gcc
CFLAGS   := -Wall -Wextra -pedantic -ggdb
CPPFLAGS := -MMD -MP

DESTDIR  := build
TARGET   := tt

SRC      := src/main.c src/parser.c src/flag.c
HDR_MODS := da ht arena
C_OBJS   := $(patsubst src/%.c,$(DESTDIR)/%.o,$(SRC))
H_OBJS   := $(patsubst %,$(DESTDIR)/%.o,$(HDR_MODS))
OBJS     := $(H_OBJS) $(C_OBJS)
DEPS     := $(OBJS:.o=.d)

.PHONY: all install clean

all: $(TARGET)

$(DESTDIR):
	mkdir -p $@

$(DESTDIR)/da.o: src/da.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DDA_IMPLEMENTATION -o $@ $<

$(DESTDIR)/ht.o: src/ht.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DHT_IMPLEMENTATION -o $@ $< 

$(DESTDIR)/arena.o: src/arena.h | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -xc -c -DARENA_IMPLEMENTATION -o $@ $<

$(DESTDIR)/%.o: src/%.c | $(DESTDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEPS)

install:
	mkdir -p $(DESTDIR)/bin
	cp tt $(DESTDIR)/bin/tt

clean:
	rm -f tt $(TEST_BIN)

