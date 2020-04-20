set(LOGTHRSOURCE_HEADERS
    logthrsource/logthrsourcedrv.h
    logthrsource/logthrfetcherdrv.h
    PARENT_SCOPE)

set(LOGTHRSOURCE_SOURCES
    logthrsource/logthrsourcedrv.c
    logthrsource/logthrfetcherdrv.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
