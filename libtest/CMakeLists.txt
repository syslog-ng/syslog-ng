set(LIBTEST_HEADERS
    config_parse_lib.h
    mock-transport.h
    msg_parse_lib.h
    persist_lib.h
    proto_lib.h
    queue_utils_lib.h
    stopwatch.h
    cr_template.h
    testutils.h
)

set(LIBTEST_SOURCES
    config_parse_lib.c
    libtest.c
    mock-transport.c
    msg_parse_lib.c
    persist_lib.c
    proto_lib.c
    queue_utils_lib.c
    stopwatch.c
    cr_template.c
    testutils.c
)

add_library(libtest STATIC ${LIBTEST_SOURCES})
target_link_libraries(libtest syslog-ng ${CRITERION_LIBRARIES})
target_include_directories(libtest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR})
