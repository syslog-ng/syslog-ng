moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

module_LTLIBRARIES := libconfgen.la
libconfgen_la_SOURCES = \
	confgen.h confgen-plugin.c
libconfgen_la_CPPFLAGS = $(AM_CPPFLAGS)
libconfgen_la_LIBADD = ../libsyslog-ng.la

include $(top_srcdir)/build/lex-rules.am
