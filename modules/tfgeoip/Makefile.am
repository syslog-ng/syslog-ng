if ENABLE_GEOIP
module_LTLIBRARIES				+= modules/tfgeoip/libtfgeoip.la

modules_tfgeoip_libtfgeoip_la_SOURCES		=	\
	modules/tfgeoip/tfgeoip.c
modules_tfgeoip_libtfgeoip_la_CFLAGS		=	\
	$(GEOIP_CFLAGS)
modules_tfgeoip_libtfgeoip_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(GEOIP_LIBS)
modules_tfgeoip_libtfgeoip_la_LDFLAGS		=	\
	$(MODULE_LDFLAGS)
modules_tfgeoip_libtfgeoip_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/tfgeoip modules/tfgeoip/ mod-tfgeoip mod-geoip:	\
	modules/tfgeoip/libtfgeoip.la
else
modules/tfgeoip modules/tfgeoip/ mod-tfgeoip mod-geoip:
endif

.PHONY: modules/tfgeoip/ mod-tfgeoip mod-geoip
