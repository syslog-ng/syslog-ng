set(LINUX_KMSG_FORMAT_SOURCES
    linux-kmsg-format.c
    linux-kmsg-format.h
    linux-kmsg-format-plugin.c
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(linux-kmsg-format SHARED ${LINUX_KMSG_FORMAT_SOURCES})
target_link_libraries(linux-kmsg-format PRIVATE syslog-ng)

install(TARGETS linux-kmsg-format LIBRARY DESTINATION lib/syslog-ng/ COMPONENT linux-kmsg)

add_test_subdirectory(tests)
