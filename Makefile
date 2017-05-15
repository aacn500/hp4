.PHONY: all clean

all: bin/hp4

clean:
	rm -r bin || true

bin:
	mkdir -p bin

bin/hp4: bin hp4.c
	gcc -Wall hp4.c -o bin/hp4 -levent
