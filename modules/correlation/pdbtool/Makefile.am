bin_PROGRAMS				+= modules/correlation/pdbtool/pdbtool

EXTRA_DIST += modules/correlation/pdbtool/CMakeLists.txt

modules_correlation_pdbtool_pdbtool_SOURCES =	\
	modules/correlation/pdbtool/pdbtool.c
modules_correlation_pdbtool_pdbtool_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/correlation	\
	-I$(top_builddir)/modules/correlation
modules_correlation_pdbtool_pdbtool_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la \
	@TOOL_DEPS_LIBS@
