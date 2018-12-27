set(TIMEUTILS_HEADERS
    timeutils/decode.h
    timeutils/timeutils.h
    PARENT_SCOPE)

set(TIMEUTILS_SOURCES
    timeutils/decode.c
    timeutils/timeutils.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
