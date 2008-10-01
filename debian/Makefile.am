EXTRA_DIST = README.Debian syslog-ng.conf.example  changelog changelog.in \
	syslog-ng.default syslog-ng.init \
	syslog-ng.logrotate syslog-ng.logrotate.example \
	syslog-ng.conf \
	syslog-ng.files \
	syslog-ng.postrm \
	syslog-ng.preinst \
	syslog-ng.postinst \
	rules control copyright \
	syslog-ng.files \
	syslog-ng.postrm \
	syslog-ng.preinst \
	syslog-ng.postinst

changelog: changelog.in
	$(top_builddir)/config.status --file $@:$<

MAINTAINERCLEANFILES=control rules copyright changelog
