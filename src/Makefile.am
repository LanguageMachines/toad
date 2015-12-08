# $Id: Makefile.am 18760 2015-10-19 15:30:37Z sloot $
# $URL: https://ticc.uvt.nl/svn/sources/Toad/branches/ToadNT/src/Makefile.am $

AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(SYSCONFDIR)\" -std=c++11

bin_PROGRAMS = makemblem makembma checkmbma checkmblem testmbma

makemblem_SOURCES = makemblem.cxx
checkmblem_SOURCES = checkmblem.cxx

makembma_SOURCES = makembma.cxx
checkmbma_SOURCES = checkmbma.cxx
testmbma_SOURCES = testmbma.cxx
