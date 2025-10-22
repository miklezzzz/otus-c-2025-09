all: solution

solution: main.c
	$(CC) $^ -o $@ -lcurl -ljson-c -Wall -Wextra -Wpedantic -std=c11

clean:
	rm -f solution core

.PHONY: all clean
