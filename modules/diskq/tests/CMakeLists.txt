add_unit_test(LIBTEST TARGET test_diskq INCLUDES "${SYSLOG_NG_DISK_INCLUDE_DIR}" DEPENDS pthread disk-buffer)
add_unit_test(LIBTEST TARGET test_diskq_full INCLUDES "${SYSLOG_NG_DISK_INCLUDE_DIR}" DEPENDS disk-buffer)
add_unit_test(LIBTEST TARGET test_reliable_backlog INCLUDES "${SYSLOG_NG_DISK_INCLUDE_DIR}" DEPENDS disk-buffer)
