set (CONFGEN_SOURCES
    confgen.h
    confgen-plugin.c
)

add_library(confgen SHARED ${CONFGEN_SOURCES})
target_include_directories (confgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(confgen PRIVATE syslog-ng)

install(TARGETS confgen LIBRARY DESTINATION lib/syslog-ng/ COMPONENT confgen)

add_test_subdirectory(tests)
