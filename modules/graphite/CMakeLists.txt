set (GRAPHITE_SOURCES
    graphite-plugin.c
    graphite-output.h
    graphite-output.c
)

add_library(graphite SHARED ${GRAPHITE_SOURCES})
target_include_directories (graphite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(graphite PRIVATE syslog-ng)

install(TARGETS graphite LIBRARY DESTINATION lib/syslog-ng/ COMPONENT graphite)

add_test_subdirectory(tests)
