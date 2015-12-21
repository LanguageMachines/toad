#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# $Id: configure.ac 18330 2015-04-21 14:16:24Z sloot $
# $URL: https://ticc.uvt.nl/svn/sources/Toad/branches/ToadNT/configure.ac $

AC_PREREQ(2.61)
AC_INIT([frogtrain],[0.1],[timbl@uvt.nl])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

if test x"${CXXFLAGS+set}" = xset; then
  # the user set CXXFLAGS; don't override it.
  cxx_flags_were_set=true
else
  cxx_flags_were_set=false
fi
if $cxx_flags_were_set; then
  CXXFLAGS=$CXXFLAGS
fi

# Checks for programs.
AC_PROG_CXX( [g++] )
AC_PROG_RANLIB
AC_PROG_LIBTOOL
LT_INIT

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

# when running tests, use CXX
AC_LANG([C++])

AX_ICU_CHECK( [4.2],
	      [CXXFLAGS="$CXXFLAGS $ICU_CPPSEARCHPATH"
	      LIBS="$ICU_LIBPATH $ICU_LIBS $ICU_IOLIBS $LIBS"],
	      [AC_MSG_FAILURE( "No ICU development environment found. Please check if libicu-dev or the like is installed" )] )

AC_DEFINE(HAVE_ICU, 1, we want to use ICU )

# Checks for header files.
AC_CHECK_HEADERS([])

PKG_PROG_PKG_CONFIG
AC_PATH_PROG(pkgconfigpath, pkg-config, NONE)
if test "$pkgconfigpath" != "NONE"; then
# ugly hack when PKG_CONFIG_PATH isn't defined.
# couldn't get it to work otherwise
  if test "x$PKG_CONFIG_PATH" = x; then
     export PKG_CONFIG_PATH=""
  fi
fi

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$XML2_LIBS $LIBS"

AC_ARG_WITH(timbl,
       [  --with-timbl=DIR       use timbl installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timbl], [timbl >= 6.4.4] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

AC_ARG_WITH(mbt,
       [  --with-mbt=DIR       use mbt installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([mbt], [mbt >= 3.2.12] )
CXXFLAGS="$CXXFLAGS $mbt_CFLAGS"
LIBS="$mbt_LIBS $LIBS"

AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.11] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

AC_ARG_WITH(folia,
       [  --with-folia=DIR       use libfolia installed in <DIR>],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([folia],[folia >= 0.11])
CXXFLAGS="$CXXFLAGS $folia_CFLAGS"
LIBS="$folia_LIBS $LIBS"

AC_ARG_WITH(frog,
       [  --with-frog=DIR       use frog installed in <DIR>],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([frog],[frog >= 0.12.18])
CXXFLAGS="$CXXFLAGS $frog_CFLAGS"
LIBS="$frog_LIBS $LIBS"


AC_OUTPUT([
  Makefile
  src/Makefile
])
