export top_srcdir
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

if ENABLE_ENV_WRAPPER
libexec_PROGRAMS = syslog-ng
sbin_PROGRAMS = syslog-ng-wrapper syslog-ng-ctl
else
sbin_PROGRAMS = syslog-ng syslog-ng-ctl
endif

syslog_ng_SOURCES = main.c

# NOTE: we're referring to the syslog-ng lib directly instead of via
# libtool, because in mixed mode libtool is not used for the final linking
# phase.  See the comment in the configure script for details.

syslog_ng_LDADD = -L../lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@
syslog_ng_LINK =  @SYSLOGNG_LINK@
syslog_ng_wrapper_SOURCES = wrapper.c

syslog_ng_ctl_SOURCES = syslog-ng-ctl.c
syslog_ng_ctl_LDADD = ../lib/gsocket.lo ../lib/gsockaddr.lo @BASE_LIBS@ @GLIB_LIBS@ @RESOLV_LIBS@


install-exec-hook:

if ENABLE_ENV_WRAPPER
	mv $(DESTDIR)/$(sbindir)/syslog-ng-wrapper $(DESTDIR)/$(sbindir)/syslog-ng
endif
	$(mkinstalldirs) $(DESTDIR)/$(localstatedir)
