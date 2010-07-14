plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

if ENABLE_SQL

plugin_LTLIBRARIES := libafsql.la
libafsql_la_SOURCES = \
	afsql.c afsql.h \
	afsql-grammar.y afsql-parser.c afsql-parser.h afsql-plugin.c

libafsql_la_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SQL=1
libafsql_la_LIBADD = $(LIBDBI_LIBS)


endif

BUILT_SOURCES = afsql-grammar.y afsql-grammar.c afsql-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsql-grammar.ym

include $(top_srcdir)/build/lex-rules.am
