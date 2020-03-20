#               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.10 foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.

# Initialize libtool                                                            
LT_PREREQ([2.2])
LT_INIT

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Set options
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [enable debugging @<:@default=no@:>@]),
              [], [enable_debug=no])
if test "$enable_debug" = "yes"; then
    CFLAGS="-g3 -O0 -fno-inline -Wno-inline $CFLAGS"
    DIRECT_BUILD_DEBUG=1
else
    DIRECT_BUILD_DEBUG=0
fi
AM_CONDITIONAL(ENABLE_DEBUG, test "$enable_debug" = "yes")

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
