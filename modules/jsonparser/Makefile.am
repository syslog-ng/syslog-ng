moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

if ENABLE_JSON_PARSE
module_LTLIBRARIES		:= libjsonparser.la
libjsonparser_la_SOURCES	= \
				jsonparser.c jsonparser.h \
				jsonparser-grammar.y \
				jsonparser-parser.c jsonparser-parser.h \
				jsonparser-plugin.c

libjsonparser_la_CPPFLAGS	= $(AM_CPPFLAGS)
libjsonparser_la_CFLAGS		= $(JSON_CFLAGS)
libjsonparser_la_LIBADD		= $(MODULE_DEPS_LIBS)
libjsonparser_la_LDFLAGS	= $(MODULE_LDFLAGS) $(JSON_LIBS)

BUILT_SOURCES			= jsonparser-grammar.y jsonparser-grammar.c jsonparser-grammar.h
EXTRA_DIST			= $(BUILT_SOURCES) jsonparser-grammar.ym
endif

include $(top_srcdir)/build/lex-rules.am
