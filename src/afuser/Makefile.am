plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libafuser.la
libafuser_la_SOURCES = \
	afuser.c afuser.h \
	afuser-grammar.y afuser-parser.c afuser-parser.h afuser-plugin.c

libafuser_la_CPPFLAGS = $(AM_CPPFLAGS)

include $(top_srcdir)/build/lex-rules.am
