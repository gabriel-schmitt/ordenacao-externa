CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -std=c99 -pedantic
TARGETS=intercalate partition

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean

clean:
	rm $(TARGETS)
