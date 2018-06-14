if ENABLE_GEOIP
module_LTLIBRARIES				+= modules/geoip/libgeoip-plugin.la

modules_geoip_libgeoip_plugin_la_SOURCES=	\
	modules/geoip/geoip-parser.c		\
	modules/geoip/geoip-parser.h		\
	modules/geoip/geoip-parser-grammar.y	\
	modules/geoip/geoip-parser-parser.c	\
	modules/geoip/geoip-parser-parser.h	\
	modules/geoip/geoip-plugin.c		\
	modules/geoip/geoip-helper.h		\
	modules/geoip/geoip-helper.c

modules_geoip_libgeoip_plugin_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/geoip			\
	-I$(top_builddir)/modules/geoip
modules_geoip_libgeoip_plugin_la_CFLAGS		=	\
	$(AM_CFLAGS) \
	$(GEOIP_CFLAGS)
modules_geoip_libgeoip_plugin_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(GEOIP_LIBS)
modules_geoip_libgeoip_plugin_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_geoip_libgeoip_plugin_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/geoip modules/geoip/ mod-geoip:	\
	modules/geoip/libgeoip-plugin.la
else
modules/geoip modules/geoip/ mod-geoip:
endif

BUILT_SOURCES					+=	\
	modules/geoip/geoip-parser-grammar.y		\
	modules/geoip/geoip-parser-grammar.c		\
	modules/geoip/geoip-parser-grammar.h
EXTRA_DIST					+=	\
	modules/geoip/geoip-parser-grammar.ym		\
	modules/geoip/tfgeoip.c	\
	modules/geoip/CMakeLists.txt

.PHONY: modules/geoip/ mod-geoip

include modules/geoip/tests/Makefile.am
