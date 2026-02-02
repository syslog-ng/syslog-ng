# File: modules/secure-logging/slogverify/Makefile.am

if ENABLE_SLOG

bin_PROGRAMS += modules/secure-logging/slogverify/slogverify

modules_secure_logging_slogverify_slogverify_SOURCES = \
	modules/secure-logging/slogverify/slogverify.c

modules_secure_logging_slogverify_slogverify_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging

modules_secure_logging_slogverify_slogverify_LDFLAGS = \
	$(AM_LDFLAGS) \
	-Wl,-rpath,'$(libdir)' \
	-Wl,--disable-new-dtags \
	-L$(top_builddir)/modules/secure-logging \
	-Wl,--as-needed

modules_secure_logging_slogverify_slogverify_LDADD = \
	$(top_builddir)/lib/libsyslog-ng.la \
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la \
	-lsecret-storage \
	@TOOL_DEPS_LIBS@

modules_secure_logging_slogverify_slogverify_DEPENDENCIES = \
	$(top_builddir)/lib/libsyslog-ng.la \
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la

endif

EXTRA_DIST += modules/secure-logging/slogverify/CMakeLists.txt
