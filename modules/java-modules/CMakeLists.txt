if (ENABLE_JAVA_MODULES AND NOT ENABLE_JAVA)
  message(FATAL_ERROR "Java based modules enabled, but the required syslog-ng Java plugin is disabled.")
endif ()

find_package (Gradle 3.4)

if (ENABLE_JAVA_MODULES AND NOT GRADLE_FOUND)
  message(FATAL_ERROR "Java based modules enabled, but the required gardle dependency is not found.")
endif ()

set (CAN_ENABLE_JAVA_MODULES OFF)
if ((GRADLE_FOUND AND ENABLE_JAVA))
  set (CAN_ENABLE_JAVA_MODULES ON)
endif ()

module_switch (ENABLE_JAVA_MODULES "Enable Java modules" CAN_ENABLE_JAVA_MODULES)

if (NOT ENABLE_JAVA_MODULES)
  return ()
endif ()

set(GRADLE_WORKDIR ${CMAKE_CURRENT_BINARY_DIR}/.gradle)
set(SYSLOG_DEPS_DIR ${PROJECT_BINARY_DIR}/modules/java)
set(JAVA_MOD_DST_DIR "${CMAKE_INSTALL_PREFIX}/lib/syslog-ng/java-modules")

add_custom_target(BuildJavaModules ALL
  COMMAND ${GRADLE_EXECUTABLE} --project-cache-dir ${GRADLE_WORKDIR} -g ${GRADLE_WORKDIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -PsyslogBuildDir=${CMAKE_CURRENT_BINARY_DIR} -PsyslogDepsDir=${SYSLOG_DEPS_DIR} build copyJars
)
add_dependencies(BuildJavaModules mod-java)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/built-jars/ DESTINATION lib/syslog-ng/java-modules)
install(CODE "execute_process(COMMAND ${GRADLE_EXECUTABLE} --project-cache-dir ${GRADLE_WORKDIR} -g ${GRADLE_WORKDIR} -p ${CMAKE_CURRENT_SOURCE_DIR} -PsyslogBuildDir=${CMAKE_CURRENT_BINARY_DIR} -PsyslogDepsDir=${SYSLOG_DEPS_DIR} -PjarDestDir=${JAVA_MOD_DST_DIR} copyLog4j)")

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ".gradle")
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "built-jars")
