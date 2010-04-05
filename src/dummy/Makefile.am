plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
plugin_LTLIBRARIES = libdummy.la

export top_srcdir

libdummy_la_SOURCES = dummy-grammar.y dummy.c dummy-parser.c dummy-parser.h

dummy-grammar.y: dummy-grammar.y.in ../merge-grammar.pl ../cfg-grammar.y
	$(top_srcdir)/src/merge-grammar.pl $(srcdir)/dummy-grammar.y.in > $@


dummy-grammar.h: dummy-grammar.y

dummy-grammar.c: dummy-grammar.y

MAINTAINER_CLEAN_FILES = dummy-grammar.c dummy-grammar.h
