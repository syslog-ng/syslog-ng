SUBDIRS = src tests doc contrib solbuild

EXTRA_DIST = debian/README.Debian debian/copyright debian/syslog-ng.conf.example debian/syslog-ng.files debian/syslog-ng.logrotate debian/syslog-ng.postrm \
	debian/changelog debian/changelog.in debian/rules debian/syslog-ng.default debian/syslog-ng.init debian/syslog-ng.logrotate.example debian/syslog-ng.preinst \
	debian/control debian/syslog-ng.conf debian/syslog-ng.docs debian/syslog-ng.logcheck.ignore debian/syslog-ng.postinst VERSION \
	syslog-ng.spec.bb syslog-ng.spec.bb.in

debian/changelog: debian/changelog.in
	$(top_builddir)/config.status --file $@:$<
