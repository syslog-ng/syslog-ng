set(TRANSPORT_HEADERS
    transport/logtransport.h
    transport/transport-aux-data.h
    transport/transport-tls.h
    transport/transport-file.h
    transport/transport-pipe.h
    transport/transport-socket.h
    transport/transport-factory-id.h
    transport/transport-factory.h
    transport/transport-factory-registry.h
    transport/multitransport.h
    PARENT_SCOPE)

set(TRANSPORT_SOURCES
    transport/logtransport.c
    transport/transport-aux-data.c
    transport/transport-file.c
    transport/transport-pipe.c
    transport/transport-socket.c
    transport/transport-tls.c
    transport/transport-factory-id.c
    transport/transport-factory-registry.c
    transport/multitransport.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
