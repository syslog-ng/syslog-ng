set(SIGNAL_SLOT_CONNECTOR_HEADERS
    signal-slot-connector/signal-slot-connector.h
    PARENT_SCOPE)

  set(SIGNAL_SLOT_CONNECTOR_SOURCES
    signal-slot-connector/signal-slot-connector.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
