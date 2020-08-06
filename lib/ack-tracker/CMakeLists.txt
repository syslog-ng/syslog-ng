set(ACK_TRACKER_HEADERS
    ack-tracker/ack_tracker.h
    ack-tracker/bookmark.h
    ack-tracker/consecutive_ack_record_container.h
    ack-tracker/consecutive_ack_tracker.h
    PARENT_SCOPE)

  set(ACK_TRACKER_SOURCES
    ack-tracker/instant_ack_tracker.c
    ack-tracker/consecutive_ack_tracker.c
    ack-tracker/consecutive_ack_record_container_static.c
    ack-tracker/consecutive_ack_record_container_dynamic.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
