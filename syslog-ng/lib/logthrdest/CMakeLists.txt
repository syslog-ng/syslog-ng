set(LOGTHRDEST_HEADERS
    logthrdest/logthrdestdrv.h
    PARENT_SCOPE)

set(LOGTHRDEST_SOURCES
    logthrdest/logthrdestdrv.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
