EXTRA_DIST		+= \
			debian/README.Debian \
			debian/syslog-ng.conf.example \
			debian/changelog debian/changelog.in \
			debian/syslog-ng.default debian/syslog-ng.init \
			debian/syslog-ng.logrotate debian/syslog-ng.logrotate.example \
			debian/syslog-ng.conf \
			debian/syslog-ng.files \
			debian/syslog-ng.postrm \
			debian/syslog-ng.preinst \
			debian/syslog-ng.postinst \
			debian/rules debian/control debian/copyright \
			debian/syslog-ng.files \
			debian/syslog-ng.postrm \
			debian/syslog-ng.preinst \
			debian/syslog-ng.postinst

debian/changelog: debian/changelog.in
	$(top_builddir)/config.status --file $@:$<

MAINTAINERCLEANFILES	= debian/control debian/rules debian/copyright debian/changelog
