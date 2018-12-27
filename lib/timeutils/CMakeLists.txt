set(TIMEUTILS_HEADERS
    timeutils/cache.h
    timeutils/decode.h
    timeutils/names.h
    timeutils/strptime-tz.h
    timeutils/timeutils.h
    timeutils/zoneinfo.h
    timeutils/wallclocktime.h
    PARENT_SCOPE)

set(TIMEUTILS_SOURCES
    timeutils/cache.c
    timeutils/decode.c
    timeutils/names.c
    timeutils/strptime-tz.c
    timeutils/timeutils.c
    timeutils/zoneinfo.c
    timeutils/wallclocktime.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
