plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libpacctformat.la
libpacctformat_la_SOURCES = \
	pacct-format.c pacct-format.h pacct-format-plugin.c

libpacctformat_la_CPPFLAGS = $(AM_CPPFLAGS)

include $(top_srcdir)/build/lex-rules.am
