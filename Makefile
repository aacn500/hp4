.PHONY: all clean debug

all: bin/hp4

clean:
	rm -rf bin

debug:
	gcc -Wall -g -v -da -Q hp4.c parser.c -o bin/hp4 -levent -ljansson -Og

bin:
	mkdir -p bin

bin/hp4: bin hp4.c parser.c
	gcc -Wall -Wextra -Wno-unused-parameter hp4.c parser.c -o bin/hp4 -levent -ljansson
