ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src docs

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS README.md

ChangeLog: $(top_srcdir)/NEWS
	git pull; git2cl > ChangeLog
