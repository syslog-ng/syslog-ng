moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libafuser.la
libafuser_la_SOURCES = \
	afuser.c afuser.h \
	afuser-grammar.y afuser-parser.c afuser-parser.h afuser-plugin.c

libafuser_la_CPPFLAGS = $(AM_CPPFLAGS)
libafuser_la_LIBADD = $(MODULE_DEPS_LIBS)
libafuser_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libafuser-static.la
libafuser_static_la_SOURCES = $(libafuser_la_SOURCES)
libafuser_static_la_CPPFLAGS = $(libafuser_la_CPPFLAGS) -DSTATIC
libafuser_static_la_LIBADD =
libafuser_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif

BUILT_SOURCES = afuser-grammar.y afuser-grammar.c afuser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afuser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
