# $Id: Makefile.am 8367 2011-02-17 14:20:41Z sloot $
# $URL$

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src

EXTRA_DIST = bootstrap AUTHORS TODO NEWS

ChangeLog: $(top_srcdir)/NEWS
	svn update; svn2cl --include-rev
