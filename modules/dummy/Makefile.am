moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libdummy.la

export top_srcdir

libdummy_la_SOURCES = dummy-grammar.y dummy.c dummy.h dummy-parser.c dummy-parser.h
libdummy_la_LIBADD = $(MODULE_DEPS_LIBS)
libdummy_la_LDFLAGS = $(MODULE_LDFLAGS)

BUILT_SOURCES = dummy-grammar.y dummy-grammar.c dummy-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) dummy-grammar.ym

include $(top_srcdir)/build/lex-rules.am
