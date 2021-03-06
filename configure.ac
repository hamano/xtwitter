AC_PREREQ(2.59)
AC_INIT
AM_INIT_AUTOMAKE(xtwitter, 0.12)
AC_CONFIG_SRCDIR([src/xtwitter.c])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

# Checks for packages.
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(XAW, xaw7)
PKG_CHECK_MODULES(IMLIB, imlib2)
PKG_CHECK_MODULES(CURL, libcurl)
PKG_CHECK_MODULES(OAUTH, oauth)
PKG_CHECK_MODULES(JSON, json)

# Checks for libraries.
AC_CHECK_LIB([X11], [main])

# Checks for header files.
AC_PATH_X
AC_HEADER_STDC
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h pthread.h])
AC_CHECK_HEADERS(X11/Xlib.h,,
    AC_MSG_ERROR([Please install libx11 development package]))
AC_CHECK_HEADERS(X11/Intrinsic.h,,
    AC_MSG_ERROR([Please install xaw7 development package]))
AC_CHECK_HEADERS(Imlib2.h,,
    AC_MSG_ERROR([Please install libim development package]))
AC_CHECK_HEADERS(json/json.h,,
    AC_MSG_ERROR([Please install libjson development package]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([strcspn strdup strspn strstr])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
