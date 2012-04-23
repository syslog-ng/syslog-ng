moduledir = @moduledir@
export top_srcdir

if ENABLE_JSON
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib $(JSON_CFLAGS)
module_LTLIBRARIES = libtfjson.la

libtfjson_la_SOURCES = tfjson.c
libtfjson_la_LIBADD = $(MODULE_DEPS_LIBS) $(JSON_LIBS)
libtfjson_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libtfjson-static.la
libtfjson_static_la_SOURCES = $(libtfjson_la_SOURCES)
libtfjson_static_la_CPPFLAGS = $(AM_CPPFLAGS) -DSTATIC
libtfjson_static_la_LIBADD = $(JSON_LIBS)
libtfjson_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif
endif
