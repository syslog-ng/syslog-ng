if ENABLE_MQTT
noinst_LTLIBRARIES      += modules/mqtt/source/libmqtt_source.la
modules_mqtt_source_libmqtt_source_la_SOURCES = \
  modules/mqtt/source/mqtt-source.h   \
  modules/mqtt/source/mqtt-source.c

modules_mqtt_source_libmqtt_source_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)                              \
  $(LIBPAHO_MQTT_CFLAGS)                     \
  -I$(top_srcdir)/modules/mqtt/source/  \
  -I$(top_builddir)/modules/mqtt/source/ \
  -I$(top_srcdir)/modules/mqtt/ \
  -I$(top_builddir)/modules/mqtt/

modules_mqtt_source_libmqtt_source_la_LIBADD  = $(MODULE_DEPS_LIBS) $(LIBPAHO_MQTT_LIBS)
modules_mqtt_source_libmqtt_source_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_mqtt_source_libmqtt_source_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)
endif

EXTRA_DIST        +=      \
  modules/mqtt/source/CMakeLists.txt
