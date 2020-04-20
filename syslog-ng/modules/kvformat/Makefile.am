module_LTLIBRARIES				+= modules/kvformat/libkvformat.la

modules_kvformat_libkvformat_la_SOURCES=	\
	modules/kvformat/format-welf.c		\
	modules/kvformat/format-welf.h		\
	modules/kvformat/kv-parser.c		\
	modules/kvformat/kv-parser.h		\
	modules/kvformat/linux-audit-parser.c	\
	modules/kvformat/linux-audit-parser.h	\
	modules/kvformat/kv-parser-grammar.y	\
	modules/kvformat/kv-parser-parser.c		\
	modules/kvformat/kv-parser-parser.h		\
	modules/kvformat/kvformat-plugin.c

modules_kvformat_libkvformat_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/kvformat			\
	-I$(top_builddir)/modules/kvformat

modules_kvformat_libkvformat_la_LIBADD = \
	$(MODULE_DEPS_LIBS)

modules_kvformat_libkvformat_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_kvformat_libkvformat_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/kvformat modules/kvformat/ mod-kvformat:	\
	modules/kvformat/libkvformat.la

BUILT_SOURCES				+=	\
	modules/kvformat/kv-parser-grammar.y	\
	modules/kvformat/kv-parser-grammar.c	\
	modules/kvformat/kv-parser-grammar.h
EXTRA_DIST				+=	\
	modules/kvformat/kv-parser-grammar.ym	\
	modules/kvformat/CMakeLists.txt


.PHONY: modules/kvformat/ mod-kvformat

include modules/kvformat/tests/Makefile.am
