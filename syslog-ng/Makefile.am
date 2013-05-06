if ENABLE_ENV_WRAPPER
libexec_PROGRAMS		+= syslog-ng/syslog-ng
sbin_PROGRAMS			+= syslog-ng/syslog-ng-wrapper \
				   syslog-ng/syslog-ng-ctl
else
sbin_PROGRAMS			+= syslog-ng/syslog-ng syslog-ng/syslog-ng-ctl
endif

syslog_ng_syslog_ng_SOURCES	= syslog-ng/main.c

# NOTE: we're referring to the syslog-ng lib directly instead of via
# libtool, because in mixed mode libtool is not used for the final linking
# phase.  See the comment in the configure script for details.

syslog_ng_syslog_ng_LDADD		= -L${top_builddir}/lib/.libs -lsyslog-ng @SYSLOGNG_DEPS_LIBS@
syslog_ng_syslog_ng_LINK		=  @SYSLOGNG_LINK@
syslog_ng_syslog_ng_DEPENDENCIES	= lib/libsyslog-ng.la lib/libsyslog-ng-crypto.la

syslog_ng_syslog_ng_wrapper_SOURCES	= syslog-ng/wrapper.c

syslog_ng_syslog_ng_ctl_SOURCES		= \
	syslog-ng/syslog-ng-ctl.c	  \
	lib/gsocket.c			  \
	lib/gsockaddr.c
syslog_ng_syslog_ng_ctl_LDADD		= @BASE_LIBS@ @GLIB_LIBS@ @RESOLV_LIBS@
syslog_ng_syslog_ng_ctl_DEPENDENCIES	= lib/libsyslog-ng.la lib/libsyslog-ng-crypto.la

syslog-ng-install-exec-hook:
if ENABLE_ENV_WRAPPER
	mv $(DESTDIR)/$(sbindir)/syslog-ng-wrapper $(DESTDIR)/$(sbindir)/syslog-ng
endif
	$(mkinstalldirs) $(DESTDIR)/$(localstatedir)

INSTALL_EXEC_HOOKS			+= syslog-ng-install-exec-hook

syslog-ng syslog-ng/: ${libexec_PROGRAMS} ${sbin_PROGRAMS}
.PHONY: syslog-ng/
