moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libconvertfuncs.la
libconvertfuncs_la_SOURCES = convert-funcs.c
libconvertfuncs_la_CPPFLAGS = $(AM_CPPFLAGS)
libconvertfuncs_la_LIBADD = $(MODULE_DEPS_LIBS)
libconvertfuncs_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libconvertfuncs-static.la
libconvertfuncs_static_la_SOURCES = $(libconvertfuncs_la_SOURCES)
libconvertfuncs_static_la_CPPFLAGS = $(libconvertfuncs_la_CPPFLAGS) -DSTATIC
libconvertfuncs_static_la_LIBADD = $(MODULE_DEPS_LIBS)
libconvertfuncs_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif
