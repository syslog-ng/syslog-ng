

sbin_PROGRAMS			+= syslog-ng-ctl/syslog-ng-ctl

syslog_ng_ctl_syslog_ng_ctl_SOURCES		= 	\
	syslog-ng-ctl/syslog-ng-ctl.c			\
	syslog-ng-ctl/commands/commands.h		\
	syslog-ng-ctl/commands/commands.c		\
	syslog-ng-ctl/commands/config.h			\
	syslog-ng-ctl/commands/config.c			\
	syslog-ng-ctl/commands/credentials.h		\
	syslog-ng-ctl/commands/credentials.c		\
	syslog-ng-ctl/commands/verbose.h		\
	syslog-ng-ctl/commands/verbose.c		\
	syslog-ng-ctl/commands/ctl-stats.h		\
	syslog-ng-ctl/commands/ctl-stats.c		\
	syslog-ng-ctl/commands/query.h			\
	syslog-ng-ctl/commands/query.c			\
	syslog-ng-ctl/commands/license.h		\
	syslog-ng-ctl/commands/license.c		\
	syslog-ng-ctl/control-client.h			\
	syslog-ng-ctl/control-client.c

EXTRA_DIST					+=	\
	syslog-ng-ctl/control-client-unix.c

syslog_ng_ctl_syslog_ng_ctl_LDADD		= \
	$(MODULE_DEPS_LIBS) \
	$(TOOL_DEPS_LIBS) \
	$(top_builddir)/lib/secret-storage/libsecret-storage.la

syslog_ng_ctl_syslog_ng_ctl_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/syslog-ng-ctl

syslog_ng_ctl_syslog_ng_ctl_DEPENDENCIES	= lib/libsyslog-ng.la lib/secret-storage/libsecret-storage.la
