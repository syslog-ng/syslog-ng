set (SYSLOG_NG_CORE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/org/syslog_ng)

set (NATIVE_SOURCES
    native/java_machine.c
    native/java_machine.h
    native/java-class-loader.c
    native/java-class-loader.h
    native/java-destination.c
    native/java-destination.h
    native/java-parser.c
    native/java-parser.h
    native/java-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/java-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/java-grammar.h
)

set (PROXIES_SOURCES
    proxies/internal-message-sender-proxy.c
    proxies/java-destination-proxy.c
    proxies/java-destination-proxy.h
    proxies/java-logmsg-proxy.c
    proxies/java-logmsg-proxy.h
    proxies/java-template-proxy.c
    proxies/java-template-proxy.h
)

set (JAVA_SOURCES
  ${SYSLOG_NG_CORE_SRC_DIR}/InternalMessageSender.java
  ${SYSLOG_NG_CORE_SRC_DIR}/SyslogNgClassLoader.java
  ${SYSLOG_NG_CORE_SRC_DIR}/LogMessage.java
  ${SYSLOG_NG_CORE_SRC_DIR}/LogTemplate.java
  ${SYSLOG_NG_CORE_SRC_DIR}/LogPipe.java
  ${SYSLOG_NG_CORE_SRC_DIR}/LogDestination.java
  ${SYSLOG_NG_CORE_SRC_DIR}/TextLogDestination.java
  ${SYSLOG_NG_CORE_SRC_DIR}/StructuredLogDestination.java
  ${SYSLOG_NG_CORE_SRC_DIR}/DummyTextDestination.java
  ${SYSLOG_NG_CORE_SRC_DIR}/DummyStructuredDestination.java
)

set(BUILT_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogMessage.h
    ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogDestination.h
    ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogTemplate.h
    ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_InternalMessageSender.h
)

set_source_files_properties(${BUILT_HEADERS} PROPERTIES GENERATED TRUE)

function(gen_java_header OutputHeader ClassPath OutDir ClassName Depends)
  add_custom_command(
    OUTPUT ${OutputHeader}
    COMMAND javah -classpath ${ClassPath} -d ${OutDir} ${ClassName}
    DEPENDS ${Depends}
    VERBATIM
  )
endfunction()

generate_y_from_ym(modules/java/native/java-grammar modules/java/java-grammar)

bison_target(ModJavaGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/java-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/java-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

find_package(Java 1.7)

if (JAVA_FOUND AND Java_JAVAH_EXECUTABLE)
  OPTION(ENABLE_JAVA "Enable java module" ON)
else()
  OPTION(ENABLE_JAVA "Enable java module" OFF)
endif()

if (ENABLE_JAVA)

find_package(JNI)

if (Java_JAVAH_EXECUTABLE)
  OPTION(ENABLE_JAVA "Enable java module" ON)
else()
  OPTION(ENABLE_JAVA "Enable java module" OFF)
endif()

  include(UseJava)

  add_jar(syslog-ng-core
      SOURCES ${JAVA_SOURCES}
      OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
  )

  gen_java_header(
      ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogMessage.h
      ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar
      ${CMAKE_CURRENT_BINARY_DIR}
      "org.syslog_ng.LogMessage"
      "syslog-ng-core"
  )

  gen_java_header(
      ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_InternalMessageSender.h
      ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar
      ${CMAKE_CURRENT_BINARY_DIR}
      "org.syslog_ng.InternalMessageSender"
      "syslog-ng-core"
  )

  gen_java_header(
      ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogDestination.h
      ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar
      ${CMAKE_CURRENT_BINARY_DIR}
      "org.syslog_ng.LogDestination"
      "syslog-ng-core"
  )

  gen_java_header(
      ${CMAKE_CURRENT_BINARY_DIR}/org_syslog_ng_LogTemplate.h
      ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar
      ${CMAKE_CURRENT_BINARY_DIR}
      "org.syslog_ng.LogTemplate"
      "syslog-ng-core"
  )

  add_library(mod-java MODULE ${NATIVE_SOURCES} ${PROXIES_SOURCES} ${BUILT_HEADERS})
  target_include_directories (mod-java
      PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/native
  )
  target_include_directories (mod-java SYSTEM PRIVATE ${JNI_INCLUDE_DIRS})
  target_link_libraries(mod-java PRIVATE syslog-ng ${JNI_LIBRARIES})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-core.jar DESTINATION lib/syslog-ng/java-modules)
endif()
