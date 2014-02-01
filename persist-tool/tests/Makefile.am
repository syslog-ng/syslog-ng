persist_tool_tests_TESTS = \
	persist-tool/tests/test_persistable_state_presenter

check_PROGRAMS              += \
	${persist_tool_tests_TESTS}

persist_tool_tests_test_persistable_state_presenter_CFLAGS = $(TEST_CFLAGS)
persist_tool_tests_test_persistable_state_presenter_LDADD = $(TEST_LDADD) persist-tool/libpersist-tool.la $(JSON_LIBS) 
