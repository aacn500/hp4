AM_CFLAGS = -Wall -Werror -Wextra -pedantic -Wno-unused-parameter

noinst_LIBRARIES = libhp4.a
libhp4_includedir = $(includedir)/hp4

libhp4_a_SOURCES = debug.h \
                   event_handlers.h \
                   event_handlers.c \
                   parser.h \
                   parser.c \
                   pipe.h \
                   pipe.c \
                   stats.h \
                   stats.c \
                   strutil.h \
                   strutil.c

bin_PROGRAMS = hp4

hp4_LDADD = ../src/libhp4.a
hp4_CPPFLAGS = -I$(srcdir)

hp4_SOURCES = hp4.h \
              hp4.c
