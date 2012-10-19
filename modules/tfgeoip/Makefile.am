moduledir = @moduledir@
export top_srcdir

if ENABLE_GEOIP
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib $(GEOIP_CFLAGS)
module_LTLIBRARIES = libtfgeoip.la

libtfgeoip_la_SOURCES = tfgeoip.c
libtfgeoip_la_LIBADD = $(MODULE_DEPS_LIBS) $(GEOIP_LIBS)
libtfgeoip_la_LDFLAGS = $(MODULE_LDFLAGS)
endif
