set(TIMEUTILS_HEADERS
    timeutils/decode.h
    timeutils/timeutils.h
    timeutils/zoneinfo.h
    PARENT_SCOPE)

set(TIMEUTILS_SOURCES
    timeutils/decode.c
    timeutils/timeutils.c
    timeutils/zoneinfo.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
