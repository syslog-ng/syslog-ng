bin_PROGRAMS				+= modules/cryptofuncs/slogimport/slogimport

EXTRA_DIST += modules/cryptofuncs/slogimport/CMakeLists.txt

modules_cryptofuncs_slogimport_slogimport_SOURCES =	\
	modules/cryptofuncs/slogimport/slogimport.c
modules_cryptofuncs_slogimport_slogimport_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/cryptofuncs	\
	-I$(top_builddir)/modules/cryptofuncs
modules_cryptofuncs_slogimport_slogimport_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	@TOOL_DEPS_LIBS@
