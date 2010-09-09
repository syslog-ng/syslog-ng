SUBDIRS = lib modules syslog-ng scripts tests doc contrib scl debian tgz2build

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = VERSION \
	syslog-ng.spec syslog-ng.spec.in dist.conf dist.conf.in ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(cd $(top_srcdir); git log) > $@
	
