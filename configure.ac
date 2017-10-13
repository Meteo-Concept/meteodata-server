#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Meteodata], [0.7], [contact@meteo-concept.fr])
AC_CONFIG_SRCDIR([src/vantagepro2connector.h])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_CHECK_PROGS([DOT], [dot])
if test -z "$DOT";
then AC_MSG_WARN([[Graphs will not be available in Doxygen documentation]])
fi
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
   then AC_MSG_WARN([[Doxygen not found - continuing without Doxygen support]])
fi
AM_CONDITIONAL([HAVE_DOXYGEN],[test -n "$DOXYGEN"])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([Doxyfile])])
AM_CONDITIONAL([HAVE_DOT],[test -n "$DOT"])
AM_COND_IF([HAVE_DOT], [AC_SUBST(DOT_IS_AVAILABLE,["YES"])], [AC_SUBST(DOT_IS_AVAILABLE,["NO"])])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([syslog.h unistd.h cassandra.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AX_PTHREAD
AC_CHECK_LIB([cassandra],[cass_session_new])
BOOST_REQUIRE([1.52])
BOOST_SYSTEM
BOOST_ASIO
BOOST_PROGRAM_OPTIONS

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
