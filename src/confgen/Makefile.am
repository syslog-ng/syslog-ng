plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libconfgen.la
libconfgen_la_SOURCES = \
	confgen.h confgen-plugin.c
libconfgen_CPPFLAGS = $(AM_CPPFLAGS)

include $(top_srcdir)/build/lex-rules.am
