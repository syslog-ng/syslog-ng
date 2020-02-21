set (SYSTEM_SOURCE_SOURCES
    system-source.c
)

find_package (systemd)

add_module(
  TARGET system-source
  INCLUDES ${Libsystemd_INCLUDE_DIRS}
  DEPENDS ${Libsystemd_LIBRARIES}
  SOURCES ${SYSTEM_SOURCE_SOURCES}
)

install(FILES system-expand.sh DESTINATION share/syslog-ng/tools/)
