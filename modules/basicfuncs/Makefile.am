SUBDIRS = . tests

moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libbasicfuncs.la
libbasicfuncs_la_SOURCES = basic-funcs.c
libbasicfuncs_la_CPPFLAGS = $(AM_CPPFLAGS)
libbasicfuncs_la_LIBADD = $(MODULE_DEPS_LIBS)
libbasicfuncs_la_LDFLAGS = $(MODULE_LDFLAGS)
