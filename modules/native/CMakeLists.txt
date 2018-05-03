option(ENABLE_NATIVE "Enable to build native bindings (for Rust, C++)" ON)

set (NATIVE_SOURCES
    native-parser.c
    parser.c
    ${CMAKE_CURRENT_BINARY_DIR}/native-grammar.c
    )

set (NATIVE_HEADERS
    native-parser.h
    parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/native-grammar.h
    )

generate_y_from_ym (modules/native/native-grammar)
bison_target(NativeGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/native-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/native-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

if (ENABLE_NATIVE)
  set(PKG_CONFIG_EXEC_PREFIX "\${prefix}")
  set(PKG_CONFIG_LIBDIR "\${exec_prefix}/lib")
  set(PKG_CONFIG_LIBS "-L\${libdir} -lsyslog-ng-native-connector")

  configure_file (${PROJECT_SOURCE_DIR}/syslog-ng-native-connector.pc.cmake ${PROJECT_BINARY_DIR}/syslog-ng-native-connector.pc)
  install(FILES ${PROJECT_BINARY_DIR}/syslog-ng-native-connector.pc
          DESTINATION lib/pkgconfig
          COMPONENT native-connector)

  add_library(syslog-ng-native-connector STATIC ${NATIVE_SOURCES} ${NATIVE_HEADERS})
  target_include_directories (syslog-ng-native-connector PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_include_directories (syslog-ng-native-connector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  target_link_libraries(syslog-ng-native-connector PRIVATE syslog-ng)

  install(TARGETS syslog-ng-native-connector ARCHIVE DESTINATION lib/ COMPONENT native-connector)
endif()
