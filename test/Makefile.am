AM_CFLAGS = -Wall -Werror -Wextra -pedantic

TESTS = check_runner

noinst_PROGRAMS = check_runner

check_runner_SOURCES = check_main.c \
					   check_strutil.c $(top_builddir)/src/strutil.h \
					   check_parser.c  $(top_builddir)/src/parser.h
check_runner_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
check_runner_LDADD = $(top_builddir)/src/libhp4.a @CHECK_LIBS@
check_runner_LDFLAGS = -pthread
