bin_PROGRAMS += modules/secure-logging/slogverify/slogverify

EXTRA_DIST += modules/secure-logging/slogverify/CMakeLists.txt

modules_secure_logging_slogverify_slogverify_SOURCES =	\
	modules/secure-logging/slogverify/slogverify.c
modules_secure_logging_slogverify_slogverify_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/secure-logging	\
	-I$(top_builddir)/modules/secure-logging
modules_secure_logging_slogverify_slogverify_LDADD = \
	$(top_builddir)/lib/libsyslog-ng.la	     \
	$(top_builddir)/modules/secure-logging/libsyslog-ng-secure-logging.la \
	@TOOL_DEPS_LIBS@
