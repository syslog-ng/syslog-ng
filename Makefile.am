SUBDIRS = src tests doc contrib debian tgz2build

EXTRA_DIST = VERSION \
	syslog-ng.spec.bb syslog-ng.spec.bb.in dist.conf dist.conf.in ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(cd $(top_srcdir); git log) > $@
	
