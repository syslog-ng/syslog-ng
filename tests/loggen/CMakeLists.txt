# ########### loggen_helper static lib ##############
set(LOGGEN_HELPER_SOURCE
  loggen_helper.c
  loggen_helper.h
  ${PROJECT_SOURCE_DIR}/lib/crypto.c
  ${PROJECT_SOURCE_DIR}/lib/compat/openssl_support.c
  ${PROJECT_SOURCE_DIR}/lib/compat/glib.c
  )

add_library(loggen_helper SHARED ${LOGGEN_HELPER_SOURCE})

target_include_directories(loggen_helper PUBLIC
  ${CORE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/lib
  )

target_link_libraries(
  loggen_helper
  PUBLIC
  GLib::GLib
  GLib::GModule
  GLib::GThread
  OpenSSL::SSL
  )

# mirrors autotools: -release major.minor -version-info 0:0:0
set_target_properties(loggen_helper PROPERTIES
    OUTPUT_NAME "loggen_helper-${LSNG_RELEASE}"
    VERSION "0.0.0"
    SOVERSION "0")
install(TARGETS loggen_helper LIBRARY DESTINATION lib NAMELINK_SKIP)
install(CODE "file(CREATE_LINK \"libloggen_helper-${LSNG_RELEASE}.so.0.0.0\"
    \"\${CMAKE_INSTALL_PREFIX}/lib/libloggen_helper.so\" SYMBOLIC)")

# ########### loggen_plugin common static lib for plugins #####

add_library(loggen_plugin SHARED
  loggen_plugin.c
  loggen_plugin.h
)

target_link_libraries(loggen_plugin PUBLIC loggen_helper)

# mirrors autotools: -release major.minor -version-info 0:0:0
set_target_properties(loggen_plugin PROPERTIES
    OUTPUT_NAME "loggen_plugin-${LSNG_RELEASE}"
    VERSION "0.0.0"
    SOVERSION "0")
install(TARGETS loggen_plugin LIBRARY DESTINATION lib NAMELINK_SKIP)
install(CODE "file(CREATE_LINK \"libloggen_plugin-${LSNG_RELEASE}.so.0.0.0\"
    \"\${CMAKE_INSTALL_PREFIX}/lib/libloggen_plugin.so\" SYMBOLIC)")

# ########### loggen binary #################
set(LOGGEN_SOURCE
    loggen.c
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

target_link_libraries(
  loggen
  loggen_helper)

install(TARGETS loggen RUNTIME DESTINATION bin)

# ################# install dev headers ############

set(INTERFACE_HEADERS
    loggen_plugin.h
    loggen_helper.h
)

install(FILES ${INTERFACE_HEADERS} DESTINATION include/syslog-ng)

# ################# build plugins ############
set(LOGGEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(socket_plugin)
add_subdirectory(ssl_plugin)
add_test_subdirectory(tests)
