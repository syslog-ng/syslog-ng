moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libafuser.la
libafuser_la_SOURCES = \
	afuser.c afuser.h \
	afuser-grammar.y afuser-parser.c afuser-parser.h afuser-plugin.c

libafuser_la_CPPFLAGS = $(AM_CPPFLAGS)
libafuser_la_LIBADD = ../../lib/libsyslog-ng.la

BUILT_SOURCES = afuser-grammar.y afuser-grammar.c afuser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afuser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
