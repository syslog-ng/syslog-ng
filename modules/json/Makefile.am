if ENABLE_JSON
module_LTLIBRARIES			+= modules/json/libjson-plugin.la

modules_json_libjson_plugin_la_SOURCES	=	\
	modules/json/format-json.c		\
	modules/json/format-json.h		\
	modules/json/json-parser.c		\
	modules/json/json-parser.h		\
	modules/json/json-parser-grammar.y	\
	modules/json/json-parser-parser.c	\
	modules/json/json-parser-parser.h	\
	modules/json/dot-notation.c		\
	modules/json/dot-notation.h		\
	modules/json/json-plugin.c

modules_json_libjson_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/json		\
	-I$(top_builddir)/modules/json
modules_json_libjson_plugin_la_CFLAGS	= 	\
    $(AM_CFLAGS) \
	$(JSON_CFLAGS)
modules_json_libjson_plugin_la_LIBADD	= 	\
	$(MODULE_DEPS_LIBS) $(JSON_LIBS)

modules_json_libjson_plugin_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_json_libjson_plugin_la_DEPENDENCIES =	\
	$(MODULE_DEPS_LIBS) $(JSON_DEPENDENCY)

modules/json modules/json/ mod-json: modules/json/libjson-plugin.la
else
modules/json modules/json/ mod-json:
endif

BUILT_SOURCES				+=	\
	modules/json/json-parser-grammar.y	\
	modules/json/json-parser-grammar.c	\
	modules/json/json-parser-grammar.h
EXTRA_DIST				+=	\
	modules/json/json-parser-grammar.ym	\
	modules/json/CMakeLists.txt

.PHONY: modules/json/ mod-json

include modules/json/tests/Makefile.am
