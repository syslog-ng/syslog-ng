moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libbasicfuncs.la
libbasicfuncs_la_SOURCES = basic-funcs.c
libbasicfuncs_la_CPPFLAGS = $(AM_CPPFLAGS)
libbasicfuncs_la_LIBADD = $(MODULE_DEPS_LIBS)
libbasicfuncs_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libbasicfuncs-static.la
libbasicfuncs_static_la_SOURCES = $(libbasicfuncs_la_SOURCES)
libbasicfuncs_static_la_CPPFLAGS = $(libbasicfuncs_la_CPPFLAGS) -DSTATIC
libbasicfuncs_static_la_LIBADD =
libbasicfuncs_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif
