dist_sbin_SCRIPTS += \
	contrib/syslog-ng-debun

EXTRA_DIST += \
	contrib/README \
	\
	contrib/README.syslog-ng-debun \
	contrib/syslog-ng-debun \
	\
	contrib/init.d.solaris \
	contrib/init.d.HP-UX \
	contrib/init.d.RedHat \
	contrib/init.d.SuSE \
	contrib/init.d.SunOS \
	contrib/init.d.solaris \
	contrib/init.d.RedHat-7.3 \
	\
	contrib/syslog-ng.conf.HP-UX \
	contrib/syslog-ng.conf.RedHat \
	contrib/syslog-ng.conf.SunOS \
	\
	contrib/syslog2ng \
	contrib/syslog-mc \
	\
	contrib/syslog-ng.conf.doc \
	contrib/relogger.pl \
	\
	contrib/Brewfile \
	\
	contrib/fedora-packaging/syslog-ng.conf	\
	contrib/fedora-packaging/syslog-ng.init	\
	contrib/fedora-packaging/syslog-ng.sysconfig	\
	contrib/fedora-packaging/syslog-ng.logrotate    \
	\
	contrib/rhel-packaging/syslog-ng.conf  \
	contrib/rhel-packaging/syslog-ng.init  \
	contrib/rhel-packaging/syslog-ng.logrotate \
	\
	contrib/aix-packaging/syslog-ng.conf \
	\
	contrib/hpux-packaging/syslog-ng.conf \
	contrib/hpux-packaging/syslog-ng.init \
	\
	contrib/lfs-packaging/Makefile.lfs \
	contrib/lfs-packaging/README.lfs \
	contrib/lfs-packaging/syslog-ng.init.lfs \
	\
	contrib/cygwin-packaging/cygwin-postinstall \
	contrib/cygwin-packaging/syslog-ng-config \
	contrib/cygwin-packaging/syslog-ng.sh \
	\
	contrib/solaris-packaging/solaris10_install.txt \
	contrib/solaris-packaging/syslog-ng.example.xml \
	contrib/solaris-packaging/syslog-ng@default \
	contrib/solaris-packaging/syslog-ng-sol11-smf.example.xml \
	contrib/solaris-packaging/syslog-ng.method \
	\
	contrib/freebsd-packaging/syslog-ng.rc.d \
	contrib/freebsd-packaging/syslog-ng.conf.example \
	contrib/openbsd-packaging/syslog-ng.conf \
	\
	contrib/selinux/README \
	contrib/selinux/src/root_unsafe/syslog_ng.el6.fc.in \
	contrib/selinux/src/root_unsafe/syslog_ng.el789.fc.in \
	contrib/selinux/src/root_unsafe/syslog_ng.el5.fc.in \
	contrib/selinux/src/root_safe/syslog_ng.el6.fc.in \
	contrib/selinux/src/root_safe/syslog_ng.el789.fc.in \
	contrib/selinux/src/root_safe/syslog_ng.el5.fc.in \
	contrib/selinux/src/syslog_ng.el6.0to4.te.in \
	contrib/selinux/src/syslog_ng.el7.te.in \
	contrib/selinux/src/syslog_ng.el8.te.in \
	contrib/selinux/src/syslog_ng.el9.te.in \
	contrib/selinux/src/syslog_ng.module.version \
	contrib/selinux/src/syslog_ng.el6.5up.te.in \
	contrib/selinux/src/syslog_ng.el5.te.in \
	contrib/selinux/labels.txt \
	contrib/selinux/syslog_ng.sh \
	\
	contrib/apparmor/opt.syslog-ng.sbin.syslog-ng \
	\
	contrib/balabit-initscripts/init.d \
	contrib/balabit-initscripts/init.d.freebsd \
	contrib/balabit-initscripts/init-functions \
	\
	contrib/upstart/syslog-ng.conf.upstart	\
	contrib/systemd/syslog-ng@.service \
	contrib/systemd/syslog-ng@default \
	\
	contrib/scripts/config-graph-json-to-dot.py \
	\
	contrib/valgrind/syslog-ng.supp \
	\
	contrib/syslog-ng-helm-chart/.helmignore \
	contrib/syslog-ng-helm-chart/values.yaml \
	contrib/syslog-ng-helm-chart/Chart.yaml \
	contrib/syslog-ng-helm-chart/templates/NOTES.txt \
	contrib/syslog-ng-helm-chart/templates/_helpers.tpl \
	contrib/syslog-ng-helm-chart/templates/compress-job.yaml \
	contrib/syslog-ng-helm-chart/templates/config.yaml \
	contrib/syslog-ng-helm-chart/templates/deployment.yaml \
	contrib/syslog-ng-helm-chart/templates/hpa.yaml \
	contrib/syslog-ng-helm-chart/templates/pvc.yaml \
	contrib/syslog-ng-helm-chart/templates/service.yaml \
	contrib/syslog-ng-helm-chart/templates/serviceaccount.yaml \
	contrib/syslog-ng-helm-chart/templates/tests/test-connection.yaml

if ENABLE_SYSTEMD_UNIT_INSTALL
systemdsystemunit_DATA = contrib/systemd/syslog-ng@.service
endif
