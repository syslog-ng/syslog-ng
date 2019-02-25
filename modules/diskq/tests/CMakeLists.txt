add_unit_test(LIBTEST TARGET test_diskq DEPENDS pthread disk-buffer)
add_unit_test(LIBTEST TARGET test_diskq_full DEPENDS disk-buffer)
add_unit_test(LIBTEST TARGET test_reliable_backlog DEPENDS disk-buffer)
add_unit_test(LIBTEST TARGET test_diskq_truncate DEPENDS disk-buffer)
