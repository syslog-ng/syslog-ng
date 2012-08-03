export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

sbin_PROGRAMS = syslog-ng syslog-ng-ctl syslog-ng-agent

if IS_WINDOWS
syslog_ng_agent_SOURCES = winmain_agent.c winservice.h winservice.c dbghelp.h minidump.c minidump.h
syslog_ng_SOURCES = winmain.c winservice.h winservice.c dbghelp.h minidump.c minidump.h
else
syslog_ng_SOURCES = main.c
endif

EXTRA_DIST = dbghelp_32.dll dbghelp_64.dll
# NOTE: we're referring to the syslog-ng lib directly instead of via
# libtool, because in mixed mode libtool is not used for the final linking
# phase.  See the comment in the configure script for details.

if IS_WINDOWS
syslog_ng_agent_LDADD = -L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@
syslog_ng_agent_LINK =  @SYSLOGNG_LINK@
syslog_ng_agent_LDFLAGS= @syslog_ng_LDFLAGS@
endif

syslog_ng_LDADD = -L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@
syslog_ng_LINK =  @SYSLOGNG_LINK@
syslog_ng_LDFLAGS= @syslog_ng_LDFLAGS@

syslog_ng_ctl_SOURCES = syslog-ng-ctl.c ../lib/gsocket.c ../lib/gsockaddr.c
syslog_ng_ctl_LDADD =  @BASE_LIBS@ @GLIB_LIBS@ @RESOLV_LIBS@ -L../lib/.libs -lsyslog-ng

if IS_WINDOWS
install-exec-hook:
if IS_I386
	cp $(top_srcdir)/syslog-ng/dbghelp_32.dll $(DESTDIR)$(bindir)/dbghelp.dll
else
	cp $(top_srcdir)/syslog-ng/dbghelp_64.dll $(DESTDIR)$(bindir)/dbghelp.dll
endif
endif

CLEANFILES = static.c
