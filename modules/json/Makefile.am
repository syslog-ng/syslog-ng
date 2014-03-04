moduledir = @moduledir@
export top_srcdir

SUBDIRS = . tests

if !IS_WINDOWS
if ENABLE_JSON

AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib -I$(top_srcdir)/modules/json -I$(top_builddir)/modules/json $(JSON_CFLAGS)
module_LTLIBRARIES = libjson-plugin.la

libjson_plugin_la_SOURCES =  \
  format-json.c    \
  format-json.h    \
  json-parser.c    \
  json-parser.h    \
  json-parser-grammar.y  \
  json-parser-parser.c \
  json-parser-parser.h \
  dot-notation.c   \
  dot-notation.h   \
  json-plugin.c

libjson_plugin_la_LIBADD = $(MODULE_DEPS_LIBS) $(JSON_LIBS)
libjson_plugin_la_LDFLAGS = $(MODULE_LDFLAGS)

BUILT_SOURCES = json-parser-grammar.y json-parser-grammar.c json-parser-grammar.h
EXTRA_DIST = json-parser-grammar.ym

include $(top_srcdir)/build/lex-rules.am

endif
endif
