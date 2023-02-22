#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([toad],[0.9],[lamasoftware@science.ru.nl])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX( [g++] )

# use libtool
LT_INIT

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

# when running tests, use CXX
AC_LANG([C++])

AC_OPENMP
if test "x$ac_cv_prog_cxx_openmp" != "x"; then
  if test "x$ac_cv_prog_cxx_openmp" != "xunsupported"; then
    CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
    AC_DEFINE([HAVE_OPENMP], [1] , [Define to 1 if you have OpenMP] )
  else
    AC_MSG_NOTICE([We don't have OpenMP. Multithreaded operation is disabled])
  fi
fi

# Checks for header files.
AC_CHECK_HEADERS([])

PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG_PATH" = x; then
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
else
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

AC_OSX_PKG( [icu4c] )

PKG_CHECK_MODULES([ICU], [icu-uc >= 50 icu-io] )
CXXFLAGS="$CXXFLAGS $ICU_CFLAGS"
LIBS="$ICU_LIBS $LIBS"

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$XML2_LIBS $LIBS"

PKG_CHECK_MODULES([timbl], [timbl >= 6.4.8] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

PKG_CHECK_MODULES([mbt], [mbt >= 3.7] )
CXXFLAGS="$CXXFLAGS $mbt_CFLAGS"
LIBS="$mbt_LIBS $LIBS"

PKG_CHECK_MODULES([mbtserver], [mbtserver >= 0.13] )
CXXFLAGS="$CXXFLAGS $mbtserver_CFLAGS"
LIBS="$mbtserver_LIBS $LIBS"

PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.30] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

PKG_CHECK_MODULES([folia],[folia >= 1.4])
CXXFLAGS="$CXXFLAGS $folia_CFLAGS"
LIBS="$folia_LIBS $LIBS"

PKG_CHECK_MODULES([ucto],[ucto >= 0.15])
CXXFLAGS="$CXXFLAGS $ucto_CFLAGS"
LIBS="$ucto_LIBS $LIBS"

PKG_CHECK_MODULES([frog],[frog >= 0.24])
CXXFLAGS="$CXXFLAGS $frog_CFLAGS"
LIBS="$frog_LIBS $LIBS"

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  docs/Makefile
])
AC_OUTPUT
