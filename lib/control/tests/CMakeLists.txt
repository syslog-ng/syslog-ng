add_unit_test(CRITERION TARGET test_control_cmds SOURCES test_control_cmds.c control-server-dummy.c)
add_unit_test(CRITERION TARGET test_control_connection DEPENDS Threads::Threads)
