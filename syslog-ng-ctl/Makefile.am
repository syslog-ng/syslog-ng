export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

sbin_PROGRAMS = syslog-ng-ctl

syslog_ng_ctl_SOURCES = syslog-ng-ctl.c  control_client.h ../lib/gsocket.c ../lib/gsockaddr.c
if IS_WINDOWS
syslog_ng_ctl_SOURCES += control_client_windows.c
else
syslog_ng_ctl_SOURCES += control_client_posix.c
endif

syslog_ng_ctl_LDADD =  @BASE_LIBS@ @GLIB_LIBS@ @RESOLV_LIBS@ -L../lib/.libs -lsyslog-ng
