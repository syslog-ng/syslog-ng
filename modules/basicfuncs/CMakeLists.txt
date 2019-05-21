set (BASICFUNCS_SOURCES
  basic-funcs.c
)

add_library(basicfuncs SHARED ${BASICFUNCS_SOURCES})
target_include_directories (basicfuncs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(basicfuncs PRIVATE syslog-ng m)

install(TARGETS basicfuncs LIBRARY DESTINATION lib/syslog-ng/ COMPONENT basicfuncs)

add_test_subdirectory(tests)
