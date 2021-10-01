
set(MQTT_DEST_SOURCES
    mqtt-destination.c
    mqtt-destination.h
    mqtt-worker.c
    mqtt-worker.h
)

add_module(
  TARGET mqtt-destination
  DEPENDS eclipse-paho-mqtt-c::paho-mqtt3cs
  SOURCES ${MQTT_DEST_SOURCES}
  LIBRARY_TYPE STATIC # STATIC: Built as part of a larger libmqtt module
)

target_include_directories(mqtt-destination PUBLIC ${MQTT_DIR})
