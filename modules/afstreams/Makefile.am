moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

if ENABLE_SUN_STREAMS

module_LTLIBRARIES := libafstreams.la
libafstreams_la_SOURCES = \
	afstreams.c afstreams.h \
	afstreams-grammar.y afstreams-parser.c afstreams-parser.h afstreams-plugin.c

libafstreams_la_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SUN_STREAMS=1
libafstreams_la_LIBADD = $(MODULE_DEPS_LIBS)
libafstreams_la_LDFLAGS = $(MODULE_LDFLAGS)


endif

BUILT_SOURCES = afstreams-grammar.y afstreams-grammar.c afstreams-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afstreams-grammar.ym

include $(top_srcdir)/build/lex-rules.am
