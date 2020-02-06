bin_PROGRAMS				+= modules/cryptofuncs/slogverify/slogverify

EXTRA_DIST += modules/cryptofuncs/slogverify/CMakeLists.txt

modules_cryptofuncs_slogverify_slogverify_SOURCES =	\
	modules/cryptofuncs/slogverify/slogverify.c
modules_cryptofuncs_slogverify_slogverify_CPPFLAGS=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/cryptofuncs	\
	-I$(top_builddir)/modules/cryptofuncs
modules_cryptofuncs_slogverify_slogverify_LDADD	=	\
	$(top_builddir)/lib/libsyslog-ng.la	\
	@TOOL_DEPS_LIBS@
