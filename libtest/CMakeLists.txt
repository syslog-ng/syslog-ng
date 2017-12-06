set(LIBTEST_HEADERS
    config_parse_lib.h
    license_module_mock.h
    mock-transport.h
    msg_parse_lib.h
    persist_lib.h
    proto_lib.h
    queue_utils_lib.h
    stopwatch.h
    template_lib.h
    testutils.h
)

set(LIBTEST_SOURCES
    config_parse_lib.c
    libtest.c
    license_module_mock.c
    mock-transport.c
    msg_parse_lib.c
    persist_lib.c
    proto_lib.c
    queue_utils_lib.c
    stopwatch.c
    template_lib.c
    testutils.c
)

add_library(libtest STATIC ${LIBTEST_SOURCES})
target_link_libraries(libtest syslog-ng)
target_include_directories(libtest PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/libtest}")
target_include_directories(libtest PRIVATE "${CORE_INCLUDE_DIRS}")

