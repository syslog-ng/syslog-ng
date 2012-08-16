SUBDIRS = lib modules syslog-ng scripts libtest tests doc contrib scl debian tgz2build build winbuild

# tests from pe
SUBDIRS += pe-tests

ACLOCAL_AMFLAGS = -I m4 --install

EXTRA_DIST = VERSION \
	syslog-ng.spec syslog-ng.spec.in dist.conf dist.conf.in ChangeLog syslog-ng.pc.in balabit-vs-build makefile.msc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = syslog-ng.pc

.PHONY: ChangeLog

ChangeLog:
	(cd $(top_srcdir); git log) > $@
	
