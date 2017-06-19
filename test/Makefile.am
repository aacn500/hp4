TESTS = check_strutil

check_PROGRAMS = check_strutil

check_strutil_SOURCES = check_strutil.c $(top_builddir)/src/strutil.h
check_strutil_CFLAGS = @CHECK_CFLAGS@
check_strutil_LDADD = $(top_builddir)/src/libhp4.a @CHECK_LIBS@
check_strutil_LDFLAGS = -pthread
