if ENABLE_GEOIP
module_LTLIBRARIES				+= modules/geoip/libgeoip-plugin.la

modules_geoip_libgeoip_plugin_la_SOURCES=	\
	modules/geoip/tfgeoip.c
modules_geoip_libgeoip_plugin_la_CFLAGS		=	\
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

.PHONY: modules/geoip/ mod-geoip
