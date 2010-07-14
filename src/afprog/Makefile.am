plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libafprog.la
libafprog_la_SOURCES = \
	afprog.c afprog.h \
	afprog-grammar.y afprog-parser.c afprog-parser.h afprog-plugin.c

BUILT_SOURCES = afprog-grammar.y afprog-grammar.c afprog-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afprog-grammar.ym

libafprog_la_CPPFLAGS = $(AM_CPPFLAGS)

include $(top_srcdir)/build/lex-rules.am
