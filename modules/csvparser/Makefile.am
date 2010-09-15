moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libcsvparser.la
libcsvparser_la_SOURCES = \
	csvparser.c csvparser.h \
	csvparser-grammar.y csvparser-parser.c csvparser-parser.h csvparser-plugin.c

libcsvparser_la_CPPFLAGS = $(AM_CPPFLAGS)
libcsvparser_la_LIBADD = ../../lib/libsyslog-ng.la
libcsvparser_la_LDFLAGS = -avoid-version -module

BUILT_SOURCES = csvparser-grammar.y csvparser-grammar.c csvparser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) csvparser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
