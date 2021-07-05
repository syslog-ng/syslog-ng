set(SYSLOG_NG_DISK_BUFFER_SOURCES
    diskq-options.h
    diskq-options.c
    diskq-config.h
    diskq-config.c
    logqueue-disk.c
    logqueue-disk.h
    logqueue-disk-non-reliable.c
    logqueue-disk-non-reliable.h
    logqueue-disk-reliable.c
    logqueue-disk-reliable.h
    qdisk.h
    qdisk.c
)

add_library(syslog-ng-disk-buffer STATIC ${SYSLOG_NG_DISK_BUFFER_SOURCES})
target_include_directories(syslog-ng-disk-buffer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(syslog-ng-disk-buffer PUBLIC syslog-ng)

set(DISKBUFFER_SOURCES
    diskq.c
    diskq.h
    diskq-parser.c
    diskq-parser.h
    diskq-plugin.c
)

add_module(
  TARGET disk-buffer
  GRAMMAR diskq-grammar
  DEPENDS syslog-ng-disk-buffer
  SOURCES ${DISKBUFFER_SOURCES}
)

add_executable(dqtool dqtool.c)
target_link_libraries(dqtool PRIVATE eventlog syslog-ng-disk-buffer)
target_include_directories(dqtool PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS dqtool RUNTIME DESTINATION bin COMPONENT diskq)

add_test_subdirectory(tests)
