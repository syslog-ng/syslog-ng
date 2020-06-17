noinst_LTLIBRARIES += modules/secure-logging/libsyslog-ng-secure-logging.la
module_LTLIBRARIES += modules/secure-logging/libsecure-logging.la

modules_secure_logging_libsyslog_ng_secure_logging_la_SOURCES =	\
	modules/secure-logging/slog.c                           \
	modules/secure-logging/slog.h

modules_secure_logging_libsyslog_ng_secure_logging_la_CFLAGS = \
	$(AM_CFLAGS)                                 \
	-I$(top_srcdir)/modules/secure-logging	     \
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_libsyslog_ng_secure_logging_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(OPENSSL_LIBS)
modules_secure_logging_libsyslog_ng_secure_logging_la_LDFLAGS		=	\
	$(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)
modules_secure_logging_libsyslog_ng_secure_logging_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

LIBSYSLOG_SECURE_LOGGING = $(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la

modules_secure_logging_libsecure_logging_la_SOURCES = \
	modules/secure-logging/secure-logging.c

modules_secure_logging_libsecure_logging_la_CFLAGS = \
	$(AM_CFLAGS)                                 \
	-I$(top_srcdir)/modules/secure-logging	     \
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_libsecure_logging_la_LIBADD = $(MODULE_DEPS_LIBS) $(OPENSSL_LIBS) $(LIBSYSLOG_SECURE_LOGGING)
modules_secure_logging_libsecure_logging_la_LDFLAGS = $(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)
modules_secure_logging_libsecure_logging_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(LIBSYSLOG_SECURE_LOGGING)

EXTRA_DIST += modules/secure-logging/CMakeLists.txt

modules/secure-logging modules/secure-logging/ mod-secure-logging: \
	modules/secure-logging/libsyslog-ng-secure-logging.la      \
	modules/secure-logging/slogkey/slogkey                     \
	modules/secure-logging/slogencrypt/slogencrypt               \
	modules/secure-logging/slogverify/slogverify

.PHONY: modules/secure-logging/ mod-secure-logging

include modules/secure-logging/slogkey/Makefile.am
include modules/secure-logging/slogverify/Makefile.am
include modules/secure-logging/tests/Makefile.am
include modules/secure-logging/slogencrypt/Makefile.am
