option(ENABLE_GETENT "Enable getent" ON)

if (NOT ENABLE_GETENT)
  return ()
endif()

set(GETENT_SOURCES
  tfgetent.c)

add_library(getent SHARED ${GETENT_SOURCES})
target_link_libraries(getent PRIVATE syslog-ng)

install(TARGETS getent LIBRARY DESTINATION lib/syslog-ng/ COMPONENT getent)
