set(DEBUGGER_HEADERS
    debugger/debugger.h
    debugger/tracer.h
    debugger/debugger-main.h
    PARENT_SCOPE)

set(DEBUGGER_SOURCES
    debugger/debugger.c
    debugger/tracer.c
    debugger/debugger-main.c
    PARENT_SCOPE)

add_subdirectory(tests)
