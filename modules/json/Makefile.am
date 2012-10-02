SUBDIRS = . tests

moduledir = @moduledir@
export top_srcdir

if ENABLE_JSON
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libjson-plugin.la

libjson_plugin_la_SOURCES	= \
				format-json.c \
				format-json.h \
				jsonparser.c \
				jsonparser.h \
				jsonparser-grammar.y \
				jsonparser-parser.c jsonparser-parser.h \
				json-plugin.c

libjson_plugin_la_CPPFLAGS	= $(AM_CPPFLAGS)
libjson_plugin_la_CFLAGS	= $(JSON_CFLAGS)
libjson_plugin_la_LIBADD	= $(MODULE_DEPS_LIBS) $(JSON_LIBS)
libjson_plugin_la_LDFLAGS	= $(MODULE_LDFLAGS)
endif

BUILT_SOURCES			= jsonparser-grammar.y jsonparser-grammar.c jsonparser-grammar.h
EXTRA_DIST			= $(BUILT_SOURCES) jsonparser-grammar.ym

include $(top_srcdir)/build/lex-rules.am
