set (LOGGEN_SOCKET_PLUGIN_SOURCE
  socket_plugin.c)

add_library(loggen_socket_plugin
  SHARED
  ${LOGGEN_SOCKET_PLUGIN_SOURCE}
  )

target_link_libraries(loggen_socket_plugin loggen_plugin)

install(TARGETS loggen_socket_plugin LIBRARY DESTINATION ${LOGGEN_PLUGIN_INSTALL_DIR})
