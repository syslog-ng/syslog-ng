bin_PROGRAMS				+= modules/secure-logging/slogencrypt/slogencrypt

EXTRA_DIST += modules/secure-logging/slogencrypt/CMakeLists.txt

modules_secure_logging_slogencrypt_slogencrypt_SOURCES =	\
	modules/secure-logging/slogencrypt/slogencrypt.c
modules_secure_logging_slogencrypt_slogencrypt_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/secure-logging	\
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_slogencrypt_slogencrypt_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la	\
	@TOOL_DEPS_LIBS@
