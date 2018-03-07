option(ENABLE_STARDATE "Enable stardate" ON)

if (ENABLE_STARDATE)
  set(stardate_SOURCES
      stardate-plugin.c)

  add_library(stardate SHARED ${stardate_SOURCES})
  target_link_libraries(stardate PRIVATE syslog-ng m)

  install(TARGETS stardate LIBRARY DESTINATION lib/syslog-ng/ COMPONENT stardate) 

  add_test_subdirectory(tests)
endif()
