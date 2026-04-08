CC := gcc
CFLAGS := -Wall -Wextra -pedantic -O2

SRC := src/main.c src/parser.c
TEST_BIN := tests/integration/tt_test

tt: $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_BIN): $(SRC)
	@mkdir -p tests/integration
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_BIN)
	pytest -v

clean:
	rm -f tt $(TEST_BIN)
