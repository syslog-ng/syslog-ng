
macro(create_syslog_ng_core_java_native)
  #The GENERATE_NATIVE_HEADERS variable introduced only in cmake 3.11.1
  #This is just a dirty workaround for that:
  set(CMAKE_JAVA_COMPILE_FLAGS "${CMAKE_JAVA_COMPILE_FLAGS};-h;${CMAKE_CURRENT_BINARY_DIR};")

  add_jar(syslog-ng-core
      SOURCES ${JAVA_SOURCES}
      OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
      GENERATE_NATIVE_HEADERS syslog-ng-core-java-native
  )

  #The GENERATE_NATIVE_HEADERS has been introduced in cmake 3.11
  if(NOT TARGET syslog-ng-core-java-native)
    add_library(syslog-ng-core-java-native INTERFACE)
    target_include_directories(syslog-ng-core-java-native INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
    add_dependencies(syslog-ng-core-java-native syslog-ng-core)
  endif()
endmacro(create_syslog_ng_core_java_native)
