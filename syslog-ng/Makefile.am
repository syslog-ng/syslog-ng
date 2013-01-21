export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

sbin_PROGRAMS = syslog-ng syslog-ng-agent

if IS_WINDOWS
syslog_ng_agent_SOURCES = winmain_agent.c winservice.h winservice.c dbghelp.h minidump.c minidump.h syslog-ng-agent-resource.rc
syslog_ng_SOURCES = winmain.c winservice.h winservice.c dbghelp.h minidump.c minidump.h
else
syslog_ng_SOURCES = main.c
endif

EXTRA_DIST = dbghelp_32.dll dbghelp_64.dll bb.ico syslog-ng-agent-resource.rc
# NOTE: we're referring to the syslog-ng lib directly instead of via
# libtool, because in mixed mode libtool is not used for the final linking
# phase.  See the comment in the configure script for details.

if IS_WINDOWS
syslog_ng_agent_LDADD = -L../lib/.libs -lsyslog-ng -lshlwapi @SYSLOGNG_DEPS_LIBS@
syslog_ng_agent_LINK =  @SYSLOGNG_LINK@
syslog_ng_agent_LDFLAGS= @syslog_ng_LDFLAGS@
endif
syslog_ng_COMMON_LDADD=-L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@

if IS_WINDOWS
syslog_ng_LDADD = -lshlwapi $(syslog_ng_COMMON_LDADD)
else
syslog_ng_LDADD = $(syslog_ng_COMMON_LDADD)
endif

syslog_ng_LINK =  @SYSLOGNG_LINK@
syslog_ng_LDFLAGS= @syslog_ng_LDFLAGS@


if IS_WINDOWS
.rc.o:
	$(WINDRES) $< -I $(builddir) -o $@

syslog-ng-agent-resource.o: syslog-ng-agent-resource.rc
	$(WINDRES) $< -I $(builddir) -o $@


install-exec-hook:
if IS_I386
	cp $(top_srcdir)/syslog-ng/dbghelp_32.dll $(DESTDIR)$(bindir)/dbghelp.dll
else
	cp $(top_srcdir)/syslog-ng/dbghelp_64.dll $(DESTDIR)$(bindir)/dbghelp.dll
endif
endif

CLEANFILES = static.c
