#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Meteodata], [2.2], [contact@meteo-concept.fr])
AC_CANONICAL_TARGET
AC_CONFIG_SRCDIR([src/davis/vantagepro2_connector.h])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])

AC_LANG_CPLUSPLUS

# Checks for programs.
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17
# For pkg-configing
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

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
AC_CHECK_HEADERS([systemd/sd-daemon.h])
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
AX_CHECK_OPENSSL
LIBCURL_CHECK_CONFIG
BOOST_REQUIRE([1.64])
BOOST_SYSTEM
BOOST_ASIO
BOOST_PROGRAM_OPTIONS
BOOST_DEFUN([Json],
    [
        BOOST_FIND_HEADER([boost/json.hpp])
        BOOST_FIND_HEADER([boost/json/src.hpp])
    ]
)
BOOST_DEFUN([Beast],
    BOOST_FIND_HEADER([boost/beast/beast.hpp])
)
BOOST_DEFUN([Property_Tree],
    [
        BOOST_FIND_HEADER([boost/ptree/ptree.hpp])
        BOOST_FIND_HEADER([boost/ptree/xml_parser.hpp])
    ]
)
PKG_CHECK_MODULES(CASSANDRA, [cassandra])
PKG_CHECK_MODULES(MYSQL, [mariadb mysqlclient])
PKG_CHECK_MODULES(SYSTEMD, [libsystemd])
PKG_CHECK_MODULES(MOSQUITTO, [libmosquitto])
PKG_CHECK_MODULES(CASSOBS, [cassobs])
PKG_CHECK_MODULES(DATE, [date])

# Define more params
AC_DEFINE(CONTROL_SOCKET_PATH, "/var/run/meteodata/control.sock", "The path where the UNIX socket used by meteodatactl will be created and looked for")
AC_DEFINE(SOCKET_LOCK_PATH, "/var/run/meteodata/meteodata.lock", "The path that meteodata-server will try to lock to make sure it owns the control socket")

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
