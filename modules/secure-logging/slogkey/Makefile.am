# File: syslog-ng/modules/secure-logging/slogkey/Makefile.am

if ENABLE_SLOG

bin_PROGRAMS += modules/secure-logging/slogkey/slogkey

modules_secure_logging_slogkey_slogkey_SOURCES = \
	modules/secure-logging/slogkey/slogkey.c

modules_secure_logging_slogkey_slogkey_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging

modules_secure_logging_slogkey_slogkey_LDFLAGS = \
	$(AM_LDFLAGS) \
	-Wl,-rpath,'$(libdir)' \
	-Wl,--disable-new-dtags \
	-L$(top_builddir)/modules/secure-logging \
	-Wl,--as-needed

modules_secure_logging_slogkey_slogkey_LDADD = \
	$(top_builddir)/lib/libsyslog-ng.la \
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la \
	-lsecret-storage \
	@TOOL_DEPS_LIBS@

modules_secure_logging_slogkey_slogkey_DEPENDENCIES = \
	$(top_builddir)/lib/libsyslog-ng.la \
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la

endif

EXTRA_DIST += modules/secure-logging/slogkey/CMakeLists.txt

