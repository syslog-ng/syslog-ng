export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib -I$(top_srcdir)/modules/agent-config

bin_PROGRAMS = upgradetool

upgradetool_SOURCES = upgradetool.c gpo_utils.h upgrade_log.c upgrade_log.h
if IS_WINDOWS
upgradetool_SOURCES += gpo_utils.c state_upgrade.c state_upgrade.h
endif
upgradetool_LDADD = $(SYSLOG_NG_LIBS) $(top_builddir)/modules/agent-config/libagent-config.la
if IS_WINDOWS
upgradetool_LDADD += -lshlwapi
endif
