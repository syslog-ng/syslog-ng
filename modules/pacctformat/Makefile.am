moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

if ENABLE_PACCT

module_LTLIBRARIES := libpacctformat.la
libpacctformat_la_SOURCES = \
	pacct-format.c pacct-format.h pacct-format-plugin.c

libpacctformat_la_CPPFLAGS = $(AM_CPPFLAGS)
libpacctformat_la_LIBADD = $(MODULE_DEPS_LIBS)
libpacctformat_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libpacctformat-static.la
libpacctformat_static_la_SOURCES = $(libpacctformat_la_SOURCES)
libpacctformat_static_la_CPPFLAGS = $(libpacctformat_la_CPPFLAGS) -DSTATIC
libpacctformat_static_la_LIBADD =
libpacctformat_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif
endif

include $(top_srcdir)/build/lex-rules.am
