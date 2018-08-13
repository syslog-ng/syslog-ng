set(LOGTHRSOURCE_HEADERS
    logthrsource/logthrsourcedrv.h
    PARENT_SCOPE)

set(LOGTHRSOURCE_SOURCES
    logthrsource/logthrsourcedrv.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
