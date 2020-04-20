loggenplugin_LTLIBRARIES				+= tests/loggen/socket_plugin/libloggen_socket_plugin.la
tests_loggen_socket_plugin_libloggen_socket_plugin_la_SOURCES	=	\
	tests/loggen/socket_plugin/socket_plugin.c	\
	tests/loggen/loggen_plugin.h \
	tests/loggen/loggen_helper.h

tests_loggen_socket_plugin_libloggen_socket_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/tests/loggen

tests_loggen_socket_plugin_libloggen_socket_plugin_la_LIBADD = \
	@GLIB_LIBS@ \
	tests/loggen/libloggen_helper.la \
	tests/loggen/libloggen_plugin.la

tests_loggen_socket_plugin_libloggen_socket_plugin_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)

tests/loggen/socket_plugin tests/loggen/socket_plugin/: \
	tests/loggen/socket_plugin/libloggen_socket_plugin.la

.PHONY: tests/loggen/socket_plugin/
