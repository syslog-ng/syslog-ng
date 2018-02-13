set(CEF_SOURCES
    format-cef-extension.c
    format-cef-extension.h
    cef-plugin.c
)

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
add_library(cef SHARED ${CEF_SOURCES})
target_link_libraries(cef PRIVATE syslog-ng)

install(TARGETS cef LIBRARY DESTINATION lib/syslog-ng/ COMPONENT cef)

add_test_subdirectory(tests)
