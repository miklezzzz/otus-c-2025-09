all: solution

solution: main.c
	$(CC) $^ -rdynamic -o $@ -L./logger -llogger -Wall -Wextra -Wpedantic -std=c11

clean:
	rm -f solution core

.PHONY: all clean
