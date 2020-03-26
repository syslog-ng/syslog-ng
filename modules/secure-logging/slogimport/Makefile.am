bin_PROGRAMS				+= modules/secure-logging/slogimport/slogimport

EXTRA_DIST += modules/secure-logging/slogimport/CMakeLists.txt

modules_secure_logging_slogimport_slogimport_SOURCES =	\
	modules/secure-logging/slogimport/slogimport.c
modules_secure_logging_slogimport_slogimport_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/secure-logging	\
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_slogimport_slogimport_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	$(top_builddir)/modules/secure-logging/libsecure-logging.la	\
	@TOOL_DEPS_LIBS@
