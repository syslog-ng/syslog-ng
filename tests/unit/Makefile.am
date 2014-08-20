tests_unit_TESTS			=  \
	tests/unit/test_resolve_pwgr	   \
	tests/unit/test_nvtable		   \
	tests/unit/test_msgsdata	   \
	tests/unit/test_logqueue	   \
	tests/unit/test_matcher		   \
	tests/unit/test_clone_logmsg 	   \
	tests/unit/test_serialize 	   \
	tests/unit/test_msgparse	   \
	tests/unit/test_dnscache	   \
	tests/unit/test_findcrlf	   \
	tests/unit/test_tags		   \
	tests/unit/test_logwriter	   \
	tests/unit/test_zone		   \
	tests/unit/test_persist_state	   \
	tests/unit/test_value_pairs     \
	tests/unit/test_value_pairs_walk   \
	tests/unit/test_ringbuffer

check_PROGRAMS				+= \
	${tests_unit_TESTS}

unit_test_extra_modules			= \
	$(PREOPEN_SYSLOGFORMAT)

tests_unit_test_resolve_pwgr_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_nvtable_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_msgsdata_CFLAGS		= $(TEST_CFLAGS)
tests_unit_test_msgsdata_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_logqueue_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_matcher_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_clone_logmsg_CFLAGS	= $(TEST_CFLAGS)
tests_unit_test_clone_logmsg_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_serialize_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_msgparse_CFLAGS		= $(TEST_CFLAGS)
tests_unit_test_msgparse_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_dnscache_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_findcrlf_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_tags_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_logwriter_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_zone_LDADD		= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_persist_state_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_value_pairs_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_value_pairs_walk_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)

tests_unit_test_ringbuffer_CFLAGS	= $(TEST_CFLAGS)
tests_unit_test_ringbuffer_LDADD	= \
	$(TEST_LDADD) $(unit_test_extra_modules)
