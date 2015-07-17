module_LTLIBRARIES				+= modules/welf/libwelf.la

modules_welf_libwelf_la_SOURCES=	\
	modules/welf/format-welf.c		\
	modules/welf/format-welf.h		\
	modules/welf/welf-plugin.c

modules_welf_libwelf_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/welf			\
	-I$(top_builddir)/modules/welf

modules_welf_libwelf_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_welf_libwelf_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/welf modules/welf/ mod-welf:	\
	modules/welf/libwelf.la

.PHONY: modules/welf/ mod-welf

include modules/welf/tests/Makefile.am
