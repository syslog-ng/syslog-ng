if (NOT BUILD_TESTING)
    return ()
endif ()

set(LIBTEST_HEADERS
    config_parse_lib.h
    fake-time.h
    mock-transport.h
    mock-cfg-parser.h
    mock-logpipe.h
    msg_parse_lib.h
    persist_lib.h
    proto_lib.h
    queue_utils_lib.h
    stopwatch.h
    cr_template.h
    grab-logging.h
)

set(LIBTEST_SOURCES
    config_parse_lib.c
    fake-time.c
    libtest.c
    mock-transport.c
    mock-cfg-parser.c
    mock-logpipe.c
    msg_parse_lib.c
    persist_lib.c
    proto_lib.c
    queue_utils_lib.c
    stopwatch.c
    cr_template.c
    grab-logging.c
)

add_library(libtest STATIC ${LIBTEST_SOURCES})
target_link_libraries(libtest syslog-ng ${CRITERION_LIBRARIES})
target_include_directories(libtest INTERFACE ${PROJECT_SOURCE_DIR})

add_test_subdirectory(tests)
