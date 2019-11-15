# ########### loggen_helper static lib ##############
set(LOGGEN_HELPER_SOURCE
  loggen_helper.c
  loggen_helper.h
  ${PROJECT_SOURCE_DIR}/lib/crypto.c
  ${PROJECT_SOURCE_DIR}/lib/compat/openssl_support.c
  )

add_library(loggen_helper STATIC ${LOGGEN_HELPER_SOURCE})

include_directories(loggen_helper PUBLIC
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/lib
  )

set_target_properties(loggen_helper
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

install(TARGETS loggen_helper ARCHIVE DESTINATION ${LOGGEN_PLUGIN_INSTALL_DIR})

# ########### loggen_plugin common static lib for plugins #####
set(LOGGEN_PLUGIN_SOURCE
  loggen_plugin.c
  loggen_plugin.h
  )

add_library(loggen_plugin STATIC ${LOGGEN_HELPER_SOURCE})

include_directories(loggen_plugin PUBLIC
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/lib
  )

set_target_properties(loggen_plugin
    PROPERTIES VERSION ${SYSLOG_NG_VERSION}
    SOVERSION ${SYSLOG_NG_VERSION})

install(TARGETS loggen_plugin ARCHIVE DESTINATION ${LOGGEN_PLUGIN_INSTALL_DIR})

# ########### loggen binary #################
set(LOGGEN_SOURCE
    loggen.c
    loggen_helper.c
    loggen_helper.h
    loggen_plugin.h
    file_reader.c
    file_reader.h
    logline_generator.c
    logline_generator.h
    ${PROJECT_SOURCE_DIR}/lib/reloc.c
    ${PROJECT_SOURCE_DIR}/lib/cache.c
    )

add_executable(loggen ${LOGGEN_SOURCE})

target_compile_definitions(loggen PUBLIC
  SYSLOG_NG_PATH_LOGGENPLUGINDIR="${LOGGEN_PLUGIN_INSTALL_DIR}"
  )

include_directories(loggen
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLIB_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/lib
  )

target_link_libraries(
  loggen
  ${GLIB_GMODULE_LIBRARIES}
  ${GLIB_GTHREAD_LIBRARIES}
  ${GLIB_LIBRARIES}
  OpenSSL::SSL
  )

install(TARGETS loggen RUNTIME DESTINATION bin)

# ################# build plugins ############
set(LOGGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(socket_plugin)
add_subdirectory(ssl_plugin)
add_subdirectory(tests)
