module_switch(ENABLE_GETENT "Enable getent")

if (NOT ENABLE_GETENT)
  return ()
endif()

set(GETENT_SOURCES
  tfgetent.c)

add_library(tfgetent SHARED ${GETENT_SOURCES})
target_link_libraries(tfgetent PRIVATE syslog-ng)

install(TARGETS tfgetent LIBRARY DESTINATION lib/syslog-ng/ COMPONENT tfgetent)
