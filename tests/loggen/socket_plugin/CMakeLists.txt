include_directories(loggen_socket_plugin
  ${CORE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${LOGGEN_INCLUDE_DIR}
  )

set (LOGGEN_SOCKET_PLUGIN_SOURCE
  socket_plugin.c
  ${LOGGEN_INCLUDE_DIR}/loggen_plugin.h
  ${LOGGEN_INCLUDE_DIR}/loggen_helper.h
  ${LOGGEN_INCLUDE_DIR}/loggen_plugin.c
  ${LOGGEN_INCLUDE_DIR}/loggen_helper.c
  )

add_library(loggen_socket_plugin
  SHARED
  ${LOGGEN_SOCKET_PLUGIN_SOURCE}
  )

target_link_libraries(
  loggen_socket_plugin
  ${GLIB_GMODULE_LIBRARIES}
  ${GLIB_GTHREAD_LIBRARIES}
  ${GLIB_LIBRARIES}
  )

set_target_properties(loggen_socket_plugin
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

install(TARGETS loggen_socket_plugin LIBRARY DESTINATION ${LOGGEN_PLUGIN_INSTALL_DIR})
