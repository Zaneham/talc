# talc -- the open TAL compiler

CC     ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2 -Isrc

SRC = $(wildcard src/*.c)

talc: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@

.PHONY: test clean
test: talc
	@sh run_tests.sh

clean:
	rm -f talc talc.exe
	rm -rf build
