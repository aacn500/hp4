sources = event_handlers.c hp4.c parser.c pipe.c stats.c strutil.c
defines = -D_GNU_SOURCE
libs = -levent -ljansson
warnings = -Wall -Wextra -Wno-unused-parameter
c_flags = $(defines) $(libs) $(warnings)
binary = bin/hp4

.PHONY: all clean debug verbose

all: bin/hp4

clean:
	rm -rf bin

debug: bin
	gcc -g -v -da -Q $(sources) -o $(binary) -Og $(c_flags) # -DHP4_DEBUG

bin:
	mkdir -p bin

verbose: bin $(sources)
	gcc $(sources) -o $(binary) $(c_flags) -DHP4_DEBUG

$(binary): bin $(sources)
	gcc $(sources) -o $(binary) $(c_flags)
