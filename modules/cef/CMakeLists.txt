set(CEF_SOURCES
    format-cef-extension.c
    format-cef-extension.h
    cef-plugin.c
)

add_library(cef SHARED ${CEF_SOURCES})
target_include_directories(cef
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(cef PRIVATE syslog-ng)

install(TARGETS cef LIBRARY DESTINATION lib/syslog-ng/ COMPONENT cef)

add_test_subdirectory(tests)
