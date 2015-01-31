EXTRA_DIST		+= \
			debian/compat \
			debian/changelog debian/changelog.in \
			debian/syslog-ng-core.syslog-ng.default debian/syslog-ng-core.syslog-ng.init \
			debian/syslog-ng-core.syslog-ng.logrotate \
			debian/syslog-ng-core.dirs \
			debian/syslog-ng-core.install \
			debian/syslog-ng-dev.install \
			debian/rules debian/control debian/copyright \
			debian/syslog-ng-core.postrm \
			debian/syslog-ng-core.preinst \
			debian/syslog-ng-core.postinst

debian/changelog: debian/changelog.in
	$(top_builddir)/config.status --file $@:$<

MAINTAINERCLEANFILES	= debian/changelog
