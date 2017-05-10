.PHONY: all clean

all: clean bin/hp4

clean:
	rm -r bin || true

bin:
	mkdir -p bin

bin/hp4: bin
	gcc -Wall hp4.c -o bin/hp4
