bin_PROGRAMS				+= modules/cryptofuncs/slogkey/slogkey

EXTRA_DIST += modules/cryptofuncs/slogkey/CMakeLists.txt

modules_cryptofuncs_slogkey_slogkey_SOURCES =	\
	modules/cryptofuncs/slogkey/slogkey.c
modules_cryptofuncs_slogkey_slogkey_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/cryptofuncs	\
	-I$(top_builddir)/modules/cryptofuncs
modules_cryptofuncs_slogkey_slogkey_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	@TOOL_DEPS_LIBS@
