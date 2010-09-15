moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libaffile.la
libaffile_la_SOURCES = \
	affile.c affile.h \
	affile-grammar.y affile-parser.c affile-parser.h affile-plugin.c

BUILT_SOURCES = affile-grammar.y affile-grammar.c affile-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) affile-grammar.ym

libaffile_la_CPPFLAGS = $(AM_CPPFLAGS)
libaffile_la_LIBADD = ../../lib/libsyslog-ng.la
libaffile_la_LDFLAGS = -avoid-version -module

include $(top_srcdir)/build/lex-rules.am
