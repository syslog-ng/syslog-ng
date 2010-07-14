plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

if ENABLE_SUN_STREAMS

plugin_LTLIBRARIES := libafstreams.la
libafstreams_la_SOURCES = \
	afstreams.c afstreams.h \
	afstreams-grammar.y afstreams-parser.c afstreams-parser.h afstreams-plugin.c

libafstreams_la_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SUN_STREAMS=1

endif

BUILT_SOURCES = afstreams-grammar.y afstreams-grammar.c afstreams-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afstreams-grammar.ym

include $(top_srcdir)/build/lex-rules.am
