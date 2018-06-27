module_LTLIBRARIES				+= modules/tagsparser/libtags-parser.la
modules_tagsparser_libtags_parser_la_SOURCES	=	\
	modules/tagsparser/tags-parser.c			\
	modules/tagsparser/tags-parser.h			\
	modules/tagsparser/tags-parser-grammar.y		\
	modules/tagsparser/tags-parser-parser.c		\
	modules/tagsparser/tags-parser-parser.h		\
	modules/tagsparser/tags-parser-plugin.c

modules_tagsparser_libtags_parser_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/tagsparser		\
	-I$(top_builddir)/modules/tagsparser
modules_tagsparser_libtags_parser_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_tagsparser_libtags_parser_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_tagsparser_libtags_parser_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=	\
	modules/tagsparser/tags-parser-grammar.y		\
	modules/tagsparser/tags-parser-grammar.c		\
	modules/tagsparser/tags-parser-grammar.h
EXTRA_DIST					+=	\
	modules/tagsparser/tags-parser-grammar.ym	\
	modules/tagsparser/CMakeLists.txt

modules/tagsparser modules/tagsparser/ mod-tags-parser: modules/tagsparser/libtags-parser.la
.PHONY: modules/tagsparser/ mod-tags-parser

include modules/tagsparser/tests/Makefile.am
