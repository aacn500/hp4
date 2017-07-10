AC_INIT([hp4],
        m4_esyscmd_s([git describe --dirty --always --tags]),
        [npg@sanger.ac.uk])

AC_PREREQ([2.68])
AC_CONFIG_AUX_DIR([build-aux])
AC_GNU_SOURCE
AC_CONFIG_SRCDIR([src/hp4.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_RANLIB

AM_PROG_AR

AC_LANG([C])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4],
                  [check_present=yes], [check_present=no])
AM_CONDITIONAL(HAVE_CHECK, test "x$check_present" = "xyes")

AC_CHECK_FUNCS([splice tee], [],
               AC_MSG_ERROR([unable to find splice and tee functions. Is this system's kernel linux >= 2.6.17?]))

AC_CHECK_LIB([event], [event_base_new], [],
             AC_MSG_ERROR([unable to find libevent]))
AC_CHECK_LIB([jansson], [json_string_length], [],
             AC_MSG_ERROR([unable to find libjansson >= 2.7]))

AC_DEFINE([PORT_DELIMITER], [":"], [char which signifies start of port name])
AC_DEFINE([STDIO_PORT], ["-"], [char which signifies port name for stdin and stdout])

dnl Lifted from https://github.com/wtsi-npg/baton/blob/b295a86cb6d31a2e62c25572af13fa23cba2c3e4/configure.ac
dnl Begin Lcov code coverage analysis

dnl Be aware on OS X that gcc may not really be gcc, but a symlink to
dnl llvm-gcc. In this case, the coverage files will not be generated
dnl unless CC is set to the real gcc when running configure.

AC_ARG_ENABLE([coverage],
                [AS_HELP_STRING([--enable-coverage],
                                    [Enable code coverage analysis (default is no)])],
                                      [coverage_enabled=${enableval}], [coverage_enabled=no])

AC_ARG_VAR([LCOV], [lcov code coverage analysis tool])
AC_ARG_VAR([GENHTML], [lcov HTML reporting tool])

AS_IF(
        [test "x${coverage_enabled}" = "xyes"],
            [AC_CHECK_LIB(gcov, __gcov_init, [], [])
                 AC_PATH_PROG([LCOV], lcov, no)
                      AS_IF([test "x${LCOV}" = "xno"],
                                         [AC_MSG_ERROR(failed to find lcov for coverage analysis)],
                                                     [])
                           AC_PATH_PROG([GENHTML], genhtml, no)
                                AS_IF([test "x${GENHTML}" = "xno"],
                                                   [AC_MSG_ERROR(failed to find genhtml for coverage reporting)],
                                                              [])],
                                                                [])

AM_CONDITIONAL(COVERAGE_ENABLED, [test "x${coverage_enabled}" = "xyes"])
dnl End Lcov

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])

AC_OUTPUT
