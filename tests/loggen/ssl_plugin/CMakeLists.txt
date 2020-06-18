include_directories(loggen_ssl_plugin
  ${CORE_INCLUDE_DIRS}
  ${LOGGEN_INCLUDE_DIR}
  )

set (LOGGEN_SSL_PLUGIN_SOURCE
  ssl_plugin.c
  ${LOGGEN_INCLUDE_DIR}/loggen_plugin.h
  ${LOGGEN_INCLUDE_DIR}/loggen_plugin.c
  )

add_library(loggen_ssl_plugin
  SHARED
  ${LOGGEN_SSL_PLUGIN_SOURCE}
  )

target_link_libraries(
  loggen_ssl_plugin
  loggen_helper
  GLib::GLib
  GLib::GModule
  GLib::GThread
  OpenSSL::SSL
  )

set_target_properties(loggen_ssl_plugin
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

install(TARGETS loggen_ssl_plugin LIBRARY DESTINATION ${LOGGEN_PLUGIN_INSTALL_DIR})
