loggenplugin_LTLIBRARIES				+= tests/loggen/ssl_plugin/libloggen_ssl_plugin.la
tests_loggen_ssl_plugin_libloggen_ssl_plugin_la_SOURCES	=	\
	tests/loggen/ssl_plugin/ssl_plugin.c 			\
	tests/loggen/loggen_plugin.h \
	tests/loggen/loggen_helper.h \
	lib/crypto.c	\
	lib/compat/openssl_support.c

tests_loggen_ssl_plugin_libloggen_ssl_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/tests/loggen

tests_loggen_ssl_plugin_libloggen_ssl_plugin_la_LDFLAGS	=	\
	-no-undefined -release ${LSNG_RELEASE} \
	-version-info ${LSNG_CURRENT}:${LSNG_REVISION}:${LSNG_AGE}

tests_loggen_ssl_plugin_libloggen_ssl_plugin_la_LIBADD = \
	@BASE_LIBS@ @OPENSSL_LIBS@ @GLIB_LIBS@ \
	tests/loggen/libloggen_helper.la \
	tests/loggen/libloggen_plugin.la


tests/loggen/ssl_plugin tests/loggen/ssl_plugin/: \
	tests/loggen/ssl_plugin/libloggen_ssl_plugin.la

.PHONY: tests/loggen/ssl_plugin/
