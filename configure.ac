#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Meteodata], [0.12], [contact@meteo-concept.fr])
AC_CONFIG_SRCDIR([src/vantagepro2connector.h])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])

AC_LANG_CPLUSPLUS

# Checks for programs.
AC_PROG_CXX
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
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([cassandra.h])
AC_CHECK_HEADERS([message.h])
AC_CHECK_HEADERS([date.h])
AC_CHECK_HEADERS([tz.h])
AC_CHECK_HEADERS([dbconnection_common.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
LT_INIT
AX_PTHREAD
AC_CHECK_LIB([cassandra],[cass_session_new])
AC_CHECK_LIB([cassobs],[main])
AC_CHECK_LIB([tz],[main])
AX_CHECK_OPENSSL
LIBCURL_CHECK_CONFIG
BOOST_REQUIRE([1.52])
BOOST_SYSTEM
BOOST_ASIO
BOOST_PROGRAM_OPTIONS
BOOST_DEFUN([Property_Tree],
[BOOST_FIND_HEADER([boost/ptree/ptree.hpp])
BOOST_FIND_HEADER([boost/ptree/xml_parser.hpp])
])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
