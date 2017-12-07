set(TRANSPORT_HEADERS
    transport/logtransport.h
    transport/transport-aux-data.h
    transport/transport-tls.h
    transport/transport-file.h
    transport/transport-pipe.h
    transport/transport-socket.h
    PARENT_SCOPE)

set(TRANSPORT_SOURCES
    transport/logtransport.c
    transport/transport-aux-data.c
    transport/transport-file.c
    transport/transport-pipe.c
    transport/transport-socket.c
    transport/transport-tls.c
    PARENT_SCOPE)

add_subdirectory(tests)
