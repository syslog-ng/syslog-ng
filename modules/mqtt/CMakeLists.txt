find_package(eclipse-paho-mqtt-c)

module_switch(ENABLE_MQTT "Enable mqtt" eclipse-paho-mqtt-c_FOUND)
if (NOT ENABLE_MQTT)
  return()
endif()

CHECK_STRUCT_HAS_MEMBER("MQTTClient_connectOptions" "httpProxy" "MQTTClient.h" SYSLOG_NG_HAVE_PAHO_HTTP_PROXY)

set(MQTT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MQTT_COMMON
    mqtt-options.c
    mqtt-options.h
)

add_subdirectory(destination)
add_subdirectory(source)

set(MQTT_SOURCES
    mqtt-parser.c
    mqtt-parser.h
    mqtt-plugins.c
)

add_module(
  TARGET mqtt
  GRAMMAR mqtt-grammar
  DEPENDS mqtt-destination
  DEPENDS mqtt-source
  SOURCES ${MQTT_SOURCES} ${MQTT_COMMON}
)