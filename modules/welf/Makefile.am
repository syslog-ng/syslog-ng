module_LTLIBRARIES				+= modules/welf/libwelf.la

modules_welf_libwelf_la_SOURCES=	\
	modules/welf/format-welf.c		\
	modules/welf/format-welf.h		\
	modules/welf/kv-parser.c		\
	modules/welf/kv-parser.h		\
	modules/welf/kv-scanner.c		\
	modules/welf/kv-scanner.h		\
	modules/welf/kv-parser-grammar.y	\
	modules/welf/kv-parser-parser.c		\
	modules/welf/kv-parser-parser.h		\
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

BUILT_SOURCES				+=	\
	modules/welf/kv-parser-grammar.y	\
	modules/welf/kv-parser-grammar.c	\
	modules/welf/kv-parser-grammar.h
EXTRA_DIST				+=	\
	modules/welf/kv-parser-grammar.ym


.PHONY: modules/welf/ mod-welf

include modules/welf/tests/Makefile.am
