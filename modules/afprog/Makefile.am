moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libafprog.la
libafprog_la_SOURCES = \
	afprog.c afprog.h \
	afprog-grammar.y afprog-parser.c afprog-parser.h afprog-plugin.c

BUILT_SOURCES = afprog-grammar.y afprog-grammar.c afprog-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afprog-grammar.ym

libafprog_la_CPPFLAGS = $(AM_CPPFLAGS)
libafprog_la_LIBADD = $(MODULE_DEPS_LIBS)
libafprog_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libafprog-static.la
libafprog_static_la_SOURCES = $(libafprog_la_SOURCES)
libafprog_static_la_CPPFLAGS = $(AM_CPPFLAGS) -DSTATIC
libafprog_static_la_LIBADD = $(MODULE_DEPS_LIBS)
libafprog_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif

include $(top_srcdir)/build/lex-rules.am
