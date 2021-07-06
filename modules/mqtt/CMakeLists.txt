find_package(eclipse-paho-mqtt-c)

module_switch(MQTT_DESTINATION "Enable mqtt_destination" eclipse-paho-mqtt-c_FOUND)
if (NOT MQTT_DESTINATION)
  return()
endif()


add_subdirectory(destination)

set(MQTT_SOURCES
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