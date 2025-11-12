all: solution

solution: main.c
	$(CC) $^ -o $@ `pkg-config --libs --cflags gtk4` -Wall -Wextra -Wpedantic -std=c11

clean:
	rm -f solution core

.PHONY: all clean
