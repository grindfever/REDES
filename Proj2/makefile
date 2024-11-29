CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

.PHONY: project
project: $(SRC)/proj.c
	$(CC) $(CFLAGS) -o proj $^

.PHONY: clean
clean:
	rm -rf proj