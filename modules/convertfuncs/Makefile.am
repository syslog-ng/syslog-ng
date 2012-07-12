SUBDIRS = . tests
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libconvertfuncs.la
libconvertfuncs_la_SOURCES = convert-funcs.c
libconvertfuncs_la_CPPFLAGS = $(AM_CPPFLAGS)
libconvertfuncs_la_LIBADD = $(MODULE_DEPS_LIBS)
libconvertfuncs_la_LDFLAGS = $(MODULE_LDFLAGS)
