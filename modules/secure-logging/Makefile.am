module_LTLIBRARIES					+=	\
	modules/secure-logging/libsecure-logging.la

EXTRA_DIST += modules/secure-logging/CMakeLists.txt

modules_secure_logging_libsecure_logging_la_SOURCES		=	\
	modules/secure-logging/secure-logging.c                         \
	modules/secure-logging/slog.c
modules_secure_logging_libsecure_logging_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(OPENSSL_LIBS)
modules_secure_logging_libsecure_logging_la_LDFLAGS		=	\
	$(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)
modules_secure_logging_libsecure_logging_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/secure-logging modules/secure-logging/ mod-secure-logging: modules/secure-logging/libsecurelogging.la \
							  modules/secure-logging/slogkey/slogkey \
							  modules/secure-logging/slogimport/slogimport \
							  modules/secure-logging/slogverify/slogverify

.PHONY: modules/secure-logging/ mod-secure-logging

include modules/secure-logging/slogkey/Makefile.am
include modules/secure-logging/slogverify/Makefile.am
include modules/secure-logging/tests/Makefile.am
include modules/secure-logging/slogimport/Makefile.am
