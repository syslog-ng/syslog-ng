set(CONTROL_HEADERS
    control/control.h
    control/control-commands.h
    control/control-command-thread.h
    control/control-main.h
    control/control-server.h
    control/control-server-unix.h
    control/control-connection.h
    PARENT_SCOPE)

set(CONTROL_SOURCES
    control/control-commands.c
    control/control-command-thread.c
    control/control-main.c
    control/control-server.c
    control/control-server-unix.c
    control/control-connection.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
