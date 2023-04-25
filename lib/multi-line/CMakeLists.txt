set(MULTI_LINE_HEADERS
    multi-line/indented-multi-line.h
    multi-line/multi-line-factory.h
    multi-line/multi-line-logic.h
    multi-line/multi-line-pattern.h
    multi-line/smart-multi-line.h
    multi-line/regexp-multi-line.h
    PARENT_SCOPE)

set(MULTI_LINE_SOURCES
    multi-line/indented-multi-line.c
    multi-line/multi-line-factory.c
    multi-line/multi-line-logic.c
    multi-line/multi-line-pattern.c
    multi-line/smart-multi-line.c
    multi-line/regexp-multi-line.c
    PARENT_SCOPE)

install(FILES smart-multi-line.fsm DESTINATION share/syslog-ng)
add_test_subdirectory(tests)
