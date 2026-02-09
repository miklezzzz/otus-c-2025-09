all: solution

solution: main.c
	$(CC) $^ -o $@ -Wall -Wextra -Wpedantic -std=c11 -I/usr/include/postgresql -lpq -lsqlite3

clean:
	rm -f solution core

.PHONY: all clean
