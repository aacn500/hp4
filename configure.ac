AC_INIT([hp4],
        m4_esyscmd_s([git describe --dirty --always --tags]),
        [npg@sanger.ac.uk])

AC_PREREQ([2.68])
AC_CONFIG_AUX_DIR([build-aux])
AC_GNU_SOURCE
AC_CONFIG_SRCDIR([src/hp4.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_PROG_CC
AC_PROG_RANLIB

AM_PROG_AR

AC_LANG([C])

AC_CHECK_LIB([event], [event_base_new], [],
             AC_MSG_ERROR([unable to find libevent]))
AC_CHECK_LIB([jansson], [json_string_length], [],
             AC_MSG_ERROR([unable to find libjansson >= 2.7]))

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
