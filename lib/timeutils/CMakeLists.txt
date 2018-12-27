set(TIMEUTILS_HEADERS
    timeutils/decode.h
    PARENT_SCOPE)

set(TIMEUTILS_SOURCES
    timeutils/decode.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
