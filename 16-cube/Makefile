all: solution

solution: main.c
	$(CC) $^ -o $@ -Wall -Wextra -Wpedantic -std=c11 -lGL -lGLU -lglut -lm

clean:
	rm -f solution core

.PHONY: all clean
