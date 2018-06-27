module_LTLIBRARIES				+= modules/cef/libcef.la

EXTRA_DIST += modules/cef/CMakeLists.txt

modules_cef_libcef_la_SOURCES	=	\
	modules/cef/format-cef-extension.c	\
	modules/cef/format-cef-extension.h	\
	modules/cef/cef-plugin.c

modules_cef_libcef_la_CFLAGS	=	\
	$(AM_CFLAGS)					\
	-I$(top_srcdir)/modules/cef	\
	-I$(top_builddir)/modules/cef
modules_cef_libcef_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_cef_libcef_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_cef_libcef_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

modules/cef modules/cef/ mod-cef: modules/cef/libcef.la

.PHONY: modules/cef/ mod-cef

include modules/cef/tests/Makefile.am
