find_package(eclipse-paho-mqtt-c)

module_switch(ENABLE_MQTT "Enable mqtt_destination" eclipse-paho-mqtt-c_FOUND)
if (NOT ENABLE_MQTT)
  return()
endif()

CHECK_STRUCT_HAS_MEMBER("MQTTClient_connectOptions" "httpProxy" "MQTTClient.h" SYSLOG_NG_HAVE_PAHO_HTTP_PROXY)

add_subdirectory(destination)

set(MQTT_SOURCES
    mqtt-options.c
    mqtt-options.h
    mqtt-parser.c
    mqtt-parser.h
    mqtt-plugins.c
)

add_module(
  TARGET mqtt
  GRAMMAR mqtt-grammar
  DEPENDS mqtt-destination
  SOURCES ${MQTT_SOURCES}
)