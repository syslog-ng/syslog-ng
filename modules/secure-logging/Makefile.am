# modules/secure-logging/Makefile.am

if ENABLE_SLOG

# -- libutils_slog.la ---
# This is a convenience library containing shared utility functions
noinst_LTLIBRARIES += modules/secure-logging/libutils_slog.la

modules_secure_logging_libutils_slog_la_SOURCES = \
	modules/secure-logging/utils_slog.c \
	modules/secure-logging/utils_slog.h

modules_secure_logging_libutils_slog_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging

modules_secure_logging_libutils_slog_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

modules_secure_logging_libutils_slog_la_LIBADD = \
	$(GLIB_LIBS) \
	-lm

modules_secure_logging_libutils_slog_la_LDFLAGS = \
	$(MODULE_LDFLAGS)


# -- libsyslog-ng-secure-logging.la ---
# This library pulls in the utilities
noinst_LTLIBRARIES += modules/secure-logging/libsyslog-ng-secure-logging.la

modules_secure_logging_libsyslog_ng_secure_logging_la_SOURCES = \
	modules/secure-logging/slog.c \
	modules/secure-logging/slog.h

modules_secure_logging_libsyslog_ng_secure_logging_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging

modules_secure_logging_libsyslog_ng_secure_logging_la_LIBADD = \
	modules/secure-logging/libutils_slog.la \
	$(MODULE_DEPS_LIBS) \
	$(OPENSSL_LIBS)

modules_secure_logging_libsyslog_ng_secure_logging_la_LDFLAGS =	\
	$(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)



# -- libsecure-logging.la ---
# Final module, Note: libutils_slog.la MUST NOT be added here,
# as it is already inside libsyslog-ng-secure-logging.la
module_LTLIBRARIES += modules/secure-logging/libsecure-logging.la
modules_secure_logging_libsecure_logging_la_SOURCES = \
	modules/secure-logging/secure-logging.c

modules_secure_logging_libsecure_logging_la_CFLAGS = \
	$(AM_CFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging

modules_secure_logging_libsecure_logging_la_LIBADD = \
	modules/secure-logging/libsyslog-ng-secure-logging.la \
	$(MODULE_DEPS_LIBS) \
	$(OPENSSL_LIBS)

modules_secure_logging_libsecure_logging_la_LDFLAGS = \
	$(MODULE_LDFLAGS)



# -- Dependencies & Distribution ---
#
EXTRA_modules_secure_logging_libsyslog_ng_secure_logging_la_DEPENDENCIES = \
	$(MODULE_DEPS_LIBS)

EXTRA_modules_secure_logging_libsecure_logging_la_DEPENDENCIES = \
	modules/secure-logging/libsyslog-ng-secure-logging.la

# -- Custom Build Targets ---

# Use a specific alias to avoid directory name conflicts
.PHONY: mod-secure-logging-all
mod-secure-logging-all: \
	modules/secure-logging/libutils_slog.la \
	modules/secure-logging/libsyslog-ng-secure-logging.la \
	modules/secure-logging/libsecure-logging.la \
	modules/secure-logging/slogkey/slogkey \
	modules/secure-logging/slogencrypt/slogencrypt \
	modules/secure-logging/slogverify/slogverify

endif

EXTRA_DIST += modules/secure-logging/CMakeLists.txt

include modules/secure-logging/slogkey/Makefile.am
include modules/secure-logging/slogverify/Makefile.am
include modules/secure-logging/tests/Makefile.am
include modules/secure-logging/slogencrypt/Makefile.am
