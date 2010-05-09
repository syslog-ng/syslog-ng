SUBDIRS = src scripts tests doc contrib debian tgz2build

EXTRA_DIST = VERSION \
	syslog-ng.spec syslog-ng.spec.in dist.conf dist.conf.in ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(cd $(top_srcdir); git log) > $@
	
