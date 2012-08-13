SUBDIRS = tests

moduledir = @moduledir@
export top_srcdir

if ENABLE_JSON_FORMAT
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib $(JSON_CFLAGS)
module_LTLIBRARIES = libtfjson.la

libtfjson_la_SOURCES = tfjson.c
libtfjson_la_LIBADD = $(MODULE_DEPS_LIBS) $(JSON_LIBS)
libtfjson_la_LDFLAGS = $(MODULE_LDFLAGS)
endif
