bin_PROGRAMS				+= modules/secure-logging/slogkey/slogkey

EXTRA_DIST += modules/secure-logging/slogkey/CMakeLists.txt

modules_secure_logging_slogkey_slogkey_SOURCES =	\
	modules/secure-logging/slogkey/slogkey.c
modules_secure_logging_slogkey_slogkey_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/secure-logging	\
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_slogkey_slogkey_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la	\
	@TOOL_DEPS_LIBS@
