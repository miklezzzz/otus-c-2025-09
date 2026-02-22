all: solution

solution: main.c
	$(CC) $^ -o $@ -Wall -Wextra -Wpedantic -std=c11 -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

clean:
	rm -f solution core

.PHONY: all clean
