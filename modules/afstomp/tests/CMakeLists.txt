add_unit_test(LIBTEST CRITERION TARGET test_stomp_proto DEPENDS afstomp INCLUDES "${AFSTOMP_INCLUDE_DIR}")
add_unit_test(LIBTEST CRITERION TARGET test_afstomp_config DEPENDS afstomp INCLUDES "${AFSTOMP_INCLUDE_DIR}")
