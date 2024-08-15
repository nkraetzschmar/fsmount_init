MAKEFLAGS += --no-builtin-rules

NOLIBC_INCLUDE_DIR := /opt/nolibc/include
NOLIBC_CFLAGS := -fno-asynchronous-unwind-tables -fno-ident -nostdlib -nostartfiles -nostdinc -isystem '$(NOLIBC_INCLUDE_DIR)'

CFLAGS := -std=c11 -O2 -static -fpie -pipe -s -Wl,--build-id=none -Wall -Wextra -Wshadow -Wdeclaration-after-statement -Werror
override CFLAGS := $(CFLAGS) $(NOLIBC_CFLAGS)

.PHONY: all clean

all: main

clean:
	rm -rf main

main: main.c
	$(CC) $(CFLAGS) -o $@ $^
