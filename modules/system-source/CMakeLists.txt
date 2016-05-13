set (SYSTEM_SOURCE_SOURCES
    system-source.c
)

find_package (systemd)

add_library (system-source MODULE ${SYSTEM_SOURCE_SOURCES})
target_include_directories (system-source PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Libsystemd_INCLUDE_DIRS})
target_link_libraries (system-source PRIVATE syslog-ng ${Libsystemd_LIBRARIES})
install(TARGETS system-source LIBRARY DESTINATION lib/syslog-ng/)
install(FILES system-expand.sh DESTINATION share/syslog-ng/tools/)
