moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

BUILT_SOURCES = afuser-grammar.y afuser-grammar.c afuser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afuser-grammar.ym

if !IS_WINDOWS
module_LTLIBRARIES := libafuser.la
libafuser_la_SOURCES = \
	afuser.c afuser.h \
	afuser-grammar.y afuser-parser.c afuser-parser.h afuser-plugin.c

libafuser_la_CPPFLAGS = $(AM_CPPFLAGS)
libafuser_la_LIBADD = $(MODULE_DEPS_LIBS)
libafuser_la_LDFLAGS = $(MODULE_LDFLAGS)
endif

include $(top_srcdir)/build/lex-rules.am
