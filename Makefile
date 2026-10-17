CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O3

SRC := src/main.c src/parser.c
TEST_BIN := tests/integration/tt_test
DESTDIR := build

tt: $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_BIN): $(SRC)
	@mkdir -p tests/integration
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_BIN)
	pytest -v

install:
	mkdir -p $(DESTDIR)/bin
	cp tt $(DESTDIR)/bin/tt

clean:
	rm -f tt $(TEST_BIN)

