.PHONY: all clean debug

all: bin/hp4 bin/parser

debug: bin parser.c
	gcc -Wall -g -v -da -Q parser.c -o bin/parser -ljansson -Og

clean:
	rm -r bin || true

bin:
	mkdir -p bin

bin/hp4: bin hp4.c
	gcc -Wall hp4.c -o bin/hp4 -levent

bin/parser: bin parser.c
	gcc -Wall parser.c -o bin/parser -ljansson
