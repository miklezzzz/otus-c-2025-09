all: solution

solution: main.c
	$(CC) $^ -o $@ -lyaml -Wall -Wextra -Wpedantic -std=c11

clean:
	rm -f solution core

.PHONY: all clean
