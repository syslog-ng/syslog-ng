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

endif

include $(top_srcdir)/build/lex-rules.am
