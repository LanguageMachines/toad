ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src docs

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS

ChangeLog: $(top_srcdir)/NEWS
	git pull; git2cl > ChangeLog
