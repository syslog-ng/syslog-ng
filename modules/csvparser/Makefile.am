moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libcsvparser.la
libcsvparser_la_SOURCES = \
	csvparser.c csvparser.h \
	csvparser-grammar.y csvparser-parser.c csvparser-parser.h csvparser-plugin.c

libcsvparser_la_CPPFLAGS = $(AM_CPPFLAGS)
libcsvparser_la_LIBADD = $(MODULE_DEPS_LIBS)
libcsvparser_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libcsvparser-static.la
libcsvparser_static_la_SOURCES = $(libcsvparser_la_SOURCES)
libcsvparser_static_la_CPPFLAGS = $(libcsvparser_la_CPPFLAGS) -DSTATIC
libcsvparser_static_la_LIBADD = $(libcsvparser_la_LIBADD)
libcsvparser_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif

BUILT_SOURCES = csvparser-grammar.y csvparser-grammar.c csvparser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) csvparser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
