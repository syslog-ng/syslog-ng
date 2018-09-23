bin_PROGRAMS				+= modules/dbparser/pdbtool/pdbtool

EXTRA_DIST += modules/dbparser/pdbtool/CMakeLists.txt

modules_dbparser_pdbtool_pdbtool_SOURCES =	\
	modules/dbparser/pdbtool/pdbtool.c
modules_dbparser_pdbtool_pdbtool_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/dbparser	\
	-I$(top_builddir)/modules/dbparser
modules_dbparser_pdbtool_pdbtool_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la \
	@TOOL_DEPS_LIBS@
