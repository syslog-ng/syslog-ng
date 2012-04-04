export top_srcdir
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

sbin_PROGRAMS = syslog-ng syslog-ng-ctl

syslog_ng_SOURCES = main.c

# NOTE: we're referring to the syslog-ng lib directly instead of via
# libtool, because in mixed mode libtool is not used for the final linking
# phase.  See the comment in the configure script for details.

syslog_ng_LDADD = -L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@ @SYSLOGNG_STATIC_MODULES@
syslog_ng_LINK =  @SYSLOGNG_LINK@
syslog_ng_LDFLAGS= @syslog_ng_LDFLAGS@

syslog_ng_ctl_SOURCES = syslog-ng-ctl.c ../lib/gsocket.c ../lib/gsockaddr.c
syslog_ng_ctl_LDADD =  @BASE_LIBS@ @GLIB_LIBS@ @RESOLV_LIBS@
