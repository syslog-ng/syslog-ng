export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

if !IS_WINDOWS
sbin_PROGRAMS = syslog-ng

syslog_ng_SOURCES = main.c

syslog_ng_COMMON_LDADD=-L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@

syslog_ng_LDADD = $(syslog_ng_COMMON_LDADD)

syslog_ng_LINK =  @SYSLOGNG_LINK@
endif
