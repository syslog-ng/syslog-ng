plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libafuser.la
libafuser_la_SOURCES = \
	afuser.c afuser.h \
	afuser-grammar.y afuser-parser.c afuser-parser.h afuser-plugin.c

libafuser_la_CPPFLAGS = $(AM_CPPFLAGS)

BUILT_SOURCES = afuser-grammar.y afuser-grammar.c afuser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afuser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
