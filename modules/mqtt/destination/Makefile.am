if ENABLE_MQTT
noinst_LTLIBRARIES      += modules/mqtt/destination/libmqtt_destination.la
modules_mqtt_destination_libmqtt_destination_la_SOURCES = \
  modules/mqtt/destination/mqtt-worker.h        \
  modules/mqtt/destination/mqtt-worker.c        \
  modules/mqtt/destination/mqtt-destination.h   \
  modules/mqtt/destination/mqtt-destination.c

modules_mqtt_destination_libmqtt_destination_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)                              \
  $(LIBPAHO_MQTT_CFLAGS)                     \
  -I$(top_srcdir)/modules/mqtt/destination/  \
  -I$(top_builddir)/modules/mqtt/destination/ \
  -I$(top_srcdir)/modules/mqtt/  \
  -I$(top_builddir)/modules/mqtt/

modules_mqtt_destination_libmqtt_destination_la_LIBADD  = $(MODULE_DEPS_LIBS) $(LIBPAHO_MQTT_LIBS)
modules_mqtt_destination_libmqtt_destination_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_mqtt_destination_libmqtt_destination_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)
endif

EXTRA_DIST        +=      \
  modules/mqtt/destination/CMakeLists.txt
