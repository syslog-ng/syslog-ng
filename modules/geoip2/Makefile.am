if ENABLE_GEOIP2
module_LTLIBRARIES				+= modules/geoip2/libgeoip2-plugin.la

modules_geoip2_libgeoip2_plugin_la_SOURCES=	\
	modules/geoip2/geoip-parser.c   \
	modules/geoip2/geoip-parser.h		\
	modules/geoip2/geoip-parser-grammar.y	\
	modules/geoip2/geoip-parser-parser.c	\
	modules/geoip2/geoip-parser-parser.h	\
	modules/geoip2/geoip-plugin.c		\
	modules/geoip2/maxminddb-helper.h	\
	modules/geoip2/maxminddb-helper.c

modules_geoip2_libgeoip2_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/geoip2			\
	-I$(top_builddir)/modules/geoip2
modules_geoip2_libgeoip2_plugin_la_CFLAGS		=	\
	$(AM_CFLAGS) \
	$(GEOIP_CFLAGS)
modules_geoip2_libgeoip2_plugin_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(MAXMINDDB_LIBS)
modules_geoip2_libgeoip2_plugin_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_geoip2_libgeoip2_plugin_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/geoip2 modules/geoip2/ mod-geoip2:	\
	modules/geoip2/libgeoip2-plugin.la
else
modules/geoip2 modules/geoip2/ mod-geoip2:
endif

BUILT_SOURCES					+=	\
	modules/geoip2/geoip-parser-grammar.y		\
	modules/geoip2/geoip-parser-grammar.c		\
	modules/geoip2/geoip-parser-grammar.h
EXTRA_DIST					+=	\
	modules/geoip2/geoip-parser-grammar.ym		\
        modules/geoip2/tfgeoip.c	\
	modules/geoip2/CMakeLists.txt

.PHONY: modules/geoip2/ mod-geoip2

include modules/geoip2/tests/Makefile.am
