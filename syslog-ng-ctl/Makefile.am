

sbin_PROGRAMS			+= syslog-ng-ctl/syslog-ng-ctl

syslog_ng_ctl_syslog_ng_ctl_SOURCES		= 	\
	syslog-ng-ctl/syslog-ng-ctl.c			\
	syslog-ng-ctl/control-client.h			\
	syslog-ng-ctl/control-client.c

EXTRA_DIST					+=	\
	syslog-ng-ctl/control-client-unix.c

syslog_ng_ctl_syslog_ng_ctl_LDADD		= \
	$(MODULE_DEPS_LIBS) \
	$(TOOL_DEPS_LIBS) \
	$(top_builddir)/lib/secret-storage/libsecret-storage.la

syslog_ng_ctl_syslog_ng_ctl_DEPENDENCIES	= lib/libsyslog-ng.la lib/secret-storage/libsecret-storage.la
