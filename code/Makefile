.PHONY: clean all

CFLAGS ?= -O3 -g -DNDEBUG -std=gnu11
LDFLAGS ?=

all: fast
fast: fast.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f fast xcheck_*.output perf.*
