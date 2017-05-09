.PHONY: all

all: bin/hp4

bin:
	mkdir -p bin

bin/hp4: bin
	gcc -Wall hp4.c -o bin/hp4
