AM_CFLAGS = -Wall -Werror -Wextra -pedantic -Wno-unused-parameter

noinst_LIBRARIES = libhp4.a
libhp4_includedir = $(includedir)/hp4
libhp4_include_HEADERS = debug.h \
                         event_handlers.h \
                         parser.h \
                         pipe.h \
                         stats.h \
                         strutil.h

libhp4_a_SOURCES =       event_handlers.c \
                         parser.c \
                         pipe.c \
                         stats.c \
                         strutil.c

bin_PROGRAMS = hp4

hp4_LDADD = ../src/libhp4.a
hp4_CPPFLAGS = -I$(srcdir)

hp4_SOURCES = hp4.h \
              hp4.c
