include modules/mqtt/destination/Makefile.am
include modules/mqtt/source/Makefile.am

if ENABLE_MQTT
MQTT_PLUGINS = \
  $(top_builddir)/modules/mqtt/destination/libmqtt_destination.la \
  $(top_builddir)/modules/mqtt/source/libmqtt_source.la


module_LTLIBRARIES += modules/mqtt/libmqtt.la

modules_mqtt_libmqtt_la_CPPFLAGS  =           \
  $(AM_CPPFLAGS)                              \
  $(LIBPAHO_MQTT_CFLAGS)                      \
  -I$(top_srcdir)/modules/mqtt/destination/   \
  -I$(top_builddir)/modules/mqtt/destination/ \
  -I$(top_srcdir)/modules/mqtt/source/   \
  -I$(top_builddir)/modules/mqtt/source/ \
  -I$(top_srcdir)/modules/mqtt/               \
  -I$(top_builddir)/modules/mqtt/

modules_mqtt_libmqtt_la_SOURCES = \
  modules/mqtt/mqtt-plugins.c     \
  modules/mqtt/mqtt-grammar.y     \
  modules/mqtt/mqtt-options.c      \
  modules/mqtt/mqtt-options.h      \
  modules/mqtt/mqtt-parser.c      \
  modules/mqtt/mqtt-parser.h

modules_mqtt_libmqtt_la_LIBADD = $(MODULE_DEPS_LIBS) $(MQTT_PLUGINS)
modules_mqtt_libmqtt_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(MQTT_PLUGINS)
modules_mqtt_libmqtt_la_LDFLAGS = $(MODULE_LDFLAGS)

modules/mqtt modules/mqtt/ mod-mqtt: modules/mqtt/libmqtt.la
endif

BUILT_SOURCES       +=         \
  modules/mqtt/mqtt-grammar.y  \
  modules/mqtt/mqtt-grammar.c  \
  modules/mqtt/mqtt-grammar.h

EXTRA_DIST += modules/mqtt/CMakeLists.txt \
              modules/mqtt/mqtt-grammar.ym

.PHONY: modules/mqtt/ mod-mqtt
