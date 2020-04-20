module_LTLIBRARIES				+= modules/csvparser/libcsvparser.la
modules_csvparser_libcsvparser_la_SOURCES	=	\
	modules/csvparser/csvparser.c			\
	modules/csvparser/csvparser.h			\
	modules/csvparser/csvparser-grammar.y		\
	modules/csvparser/csvparser-parser.c		\
	modules/csvparser/csvparser-parser.h		\
	modules/csvparser/csvparser-plugin.c

modules_csvparser_libcsvparser_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/csvparser		\
	-I$(top_builddir)/modules/csvparser		\
	-I$(top_srcdir)/lib/scanner/csv-scanner	\
	-I$(top_builddir)/lib/scanner/csv-scanner
modules_csvparser_libcsvparser_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_csvparser_libcsvparser_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_csvparser_libcsvparser_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=	\
	modules/csvparser/csvparser-grammar.y		\
	modules/csvparser/csvparser-grammar.c		\
	modules/csvparser/csvparser-grammar.h
EXTRA_DIST					+=	\
	modules/csvparser/csvparser-grammar.ym	\
	modules/csvparser/CMakeLists.txt

modules/csvparser modules/csvparser/ mod-csvparser: modules/csvparser/libcsvparser.la
.PHONY: modules/csvparser/ mod-csvparser

include modules/csvparser/tests/Makefile.am
