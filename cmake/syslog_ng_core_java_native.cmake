
macro(create_syslog_ng_core_java_native)

if ("${Java_VERSION}" VERSION_LESS 1.8)
  add_jar(syslog-ng-core
      SOURCES ${JAVA_SOURCES}
      OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
  )

  #Both exists in different version of cmake
  get_property(SYSLOG_NG_CLASSPATH TARGET syslog-ng-core PROPERTY CLASS_DIR)
  if("${SYSLOG_NG_CLASSPATH}" STREQUAL "")
    get_property(SYSLOG_NG_CLASSPATH TARGET syslog-ng-core PROPERTY CLASSDIR)
  endif()

  create_javah(GENERATED_FILES BUILT_HEADERS
        CLASSES    org.syslog_ng.LogMessage org.syslog_ng.InternalMessageSender org.syslog_ng.LogDestination org.syslog_ng.LogTemplate org.syslog_ng.LogPipe
        CLASSPATH ${SYSLOG_NG_CLASSPATH}
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS syslog-ng-core
  )

  set_source_files_properties(${BUILT_HEADERS} PROPERTIES GENERATED TRUE)
else()
  #The GENERATE_NATIVE_HEADERS variable introduced only in cmake 3.11.1
  #This is just a dirty workaround for that:
  set(CMAKE_JAVA_COMPILE_FLAGS "${CMAKE_JAVA_COMPILE_FLAGS};-h;${CMAKE_CURRENT_BINARY_DIR};")

  add_jar(syslog-ng-core
      SOURCES ${JAVA_SOURCES}
      OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
      GENERATE_NATIVE_HEADERS syslog-ng-core-headers
  )

  set(BUILT_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar")
  set_source_files_properties(${BUILT_HEADERS} PROPERTIES GENERATED TRUE)
endif()

endmacro(create_syslog_ng_core_java_native)
