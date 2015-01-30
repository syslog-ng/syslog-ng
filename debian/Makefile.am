EXTRA_DIST		+= \
			debian/compat \
			debian/changelog debian/changelog.in \
			debian/syslog-ng.default debian/syslog-ng.init \
			debian/syslog-ng.logrotate \
			debian/syslog-ng.dirs \
			debian/syslog-ng.install \
			debian/syslog-ng-dev.install \
			debian/rules debian/control debian/copyright \
			debian/syslog-ng.postrm \
			debian/syslog-ng.preinst \
			debian/syslog-ng.postinst

debian/changelog: debian/changelog.in
	$(top_builddir)/config.status --file $@:$<

MAINTAINERCLEANFILES	= debian/control debian/rules debian/copyright debian/changelog
