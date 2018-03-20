# lib loggen helper common static lib
lib_LTLIBRARIES		+= tests/loggen/libloggen_helper.la
tests_loggen_libloggen_helper_la_SOURCES	=	\
	tests/loggen/loggen_helper.c \
	tests/loggen/loggen_helper.h \
	lib/crypto.c	\
	lib/compat/openssl_support.c

tests_loggen_libloggen_helper_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/tests/loggen

tests_loggen_libloggen_helper_la_LDFLAGS	=	\
	-no-undefined -release ${LSNG_RELEASE} \
	-version-info ${LSNG_CURRENT}:${LSNG_REVISION}:${LSNG_AGE}

tests_loggen_libloggen_helper_la_LIBADD = \
	@BASE_LIBS@ @OPENSSL_LIBS@ @GLIB_LIBS@

# lib loggen plugin common static lib
lib_LTLIBRARIES		+= tests/loggen/libloggen_plugin.la
tests_loggen_libloggen_plugin_la_SOURCES	=	\
	tests/loggen/loggen_plugin.c \
	tests/loggen/loggen_plugin.h

tests_loggen_libloggen_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/tests/loggen

tests_loggen_libloggen_plugin_la_LDFLAGS	=	\
	-no-undefined -release ${LSNG_RELEASE} \
	-version-info ${LSNG_CURRENT}:${LSNG_REVISION}:${LSNG_AGE}

tests_loggen_libloggen_plugin_la_LIBADD = \
	@GLIB_LIBS@ \
	tests/loggen/libloggen_helper.la

pkginclude_HEADERS			+= \
   tests/loggen/loggen_helper.h \
   tests/loggen/loggen_plugin.h 

# binary program loggen
bin_PROGRAMS			+= tests/loggen/loggen
tests_loggen_loggen_CPPFLAGS	= \
	-I$(top_srcdir)/lib \
	-DSYSLOG_NG_PATH_LOGGEN_PLUGIN_DIR='"${loggenplugindir}"'

tests_loggen_loggen_SOURCES	=	\
	tests/loggen/loggen.c	\
	tests/loggen/loggen_helper.h \
	tests/loggen/loggen_plugin.h \
	tests/loggen/file_reader.c \
	tests/loggen/file_reader.h \
	tests/loggen/logline_generator.c \
	tests/loggen/logline_generator.h

tests_loggen_loggen_LDADD	= \
	@GLIB_LIBS@ \
	@OPENSSL_LIBS@ \
	@BASE_LIBS@ \
	tests/loggen/libloggen_helper.la

include tests/loggen/socket_plugin/Makefile.am
include tests/loggen/ssl_plugin/Makefile.am
