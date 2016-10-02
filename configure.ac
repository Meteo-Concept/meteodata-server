#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Meteodata], [0.0-r1], [contact@meteo-bretagne.fr])
AC_CONFIG_SRCDIR([src/vantagepro2connector.h])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AX_PTHREAD
BOOST_REQUIRE([1.52])
BOOST_SYSTEM
BOOST_ASIO
BOOST_PROGRAM_OPTIONS

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
