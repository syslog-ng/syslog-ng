lib_tests_TESTS		= \
	lib/tests/test_cfg_lexer_subst	\
	lib/tests/test_lexer_block	\
	lib/tests/test_cfg_tree		\
	lib/tests/test_cfg_helpers	\
	lib/tests/test_parse_number	\
	lib/tests/test_generic_number	\
	lib/tests/test_reloc		\
	lib/tests/test_hostname		\
	lib/tests/test_rcptid		\
	lib/tests/test_lexer        	\
	lib/tests/test_pragma        	\
	lib/tests/test_str_format   	\
	lib/tests/test_string_list	\
	lib/tests/test_runid        	\
	lib/tests/test_pathutils	\
	lib/tests/test_utf8utils	\
	lib/tests/test_userdb		\
	lib/tests/test_str-utils \
	lib/tests/test_atomic_gssize \
	lib/tests/test_window_size_counter \
	lib/tests/test_apphook \
	lib/tests/test_dynamic_window \
	lib/tests/test_logqueue \
	lib/tests/test_logsource \
	lib/tests/test_persist_state	\
	lib/tests/test_matcher		   \
	lib/tests/test_clone_logmsg   \
	lib/tests/test_serialize 	   \
	lib/tests/test_msgparse	   \
	lib/tests/test_dnscache	   \
	lib/tests/test_findcrlf	   \
	lib/tests/test_ringbuffer	   \
	lib/tests/test_hostid		   \
	lib/tests/test_zone		   \
	lib/tests/test_logwriter	\
	lib/tests/test_thread_wakeup	\
	lib/tests/test_logscheduler

EXTRA_DIST += lib/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_tests_TESTS}

if ENABLE_TESTING
noinst_PROGRAMS 	+= \
	lib/tests/test_host_resolve

lib_tests_test_host_resolve_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_host_resolve_LDADD	=	\
	$(TEST_LDADD)
endif

lib_tests_test_cfg_lexer_subst_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_cfg_lexer_subst_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_lexer_block_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_lexer_block_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_cfg_tree_CFLAGS		=	\
	$(TEST_CFLAGS)
lib_tests_test_cfg_tree_LDADD		=	\
	$(TEST_LDADD)

lib_tests_test_cfg_helpers_CFLAGS		=	\
	$(TEST_CFLAGS)
lib_tests_test_cfg_helpers_LDADD		=	\
	$(TEST_LDADD)


lib_tests_test_parse_number_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_parse_number_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_generic_number_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_generic_number_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_reloc_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_reloc_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_hostname_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_hostname_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_rcptid_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_rcptid_LDADD	= \
	$(TEST_LDADD)

lib_tests_test_lexer_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_lexer_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_pragma_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_pragma_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_runid_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_runid_LDADD	=	\
	$(TEST_LDADD)


lib_tests_test_str_format_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_str_format_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_string_list_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_string_list_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_pathutils_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_pathutils_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_utf8utils_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_utf8utils_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_str_utils_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_str_utils_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_atomic_gssize_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_atomic_gssize_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_window_size_counter_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_window_size_counter_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_apphook_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_apphook_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_dynamic_window_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_dynamic_window_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_logqueue_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_logqueue_LDADD = $(TEST_LDADD)

lib_tests_test_logsource_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_logsource_LDADD = $(TEST_LDADD)

lib_tests_test_logscheduler_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_logscheduler_LDADD = $(TEST_LDADD)

lib_tests_test_persist_state_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_persist_state_LDADD = $(TEST_LDADD)

CLEANFILES				+= \
	test_values.persist		   \
	test_values.persist-		   \
	test_run_id.persist		   \
	test_run_id.persist-

lib_tests_test_userdb_CFLAGS = $(TEST_CFLAGS)
lib_tests_test_userdb_LDADD	= \
	$(TEST_LDADD)

lib_tests_TESTS		+= \
	lib/tests/test_cache		\
	lib/tests/test_scratch_buffers 	\
	lib/tests/test_messages

lib_tests_test_cache_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_cache_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_scratch_buffers_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_scratch_buffers_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_messages_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_messages_LDADD	=	\
	$(TEST_LDADD)


lib_tests_test_logwriter_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)
lib_tests_test_logwriter_CFLAGS	= $(TEST_CFLAGS)

lib_tests_test_matcher_CFLAGS		= $(TEST_CFLAGS)
lib_tests_test_matcher_LDADD		= $(TEST_LDADD)

lib_tests_test_clone_logmsg_CFLAGS	= $(TEST_CFLAGS)
lib_tests_test_clone_logmsg_LDADD	= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_serialize_CFLAGS	= $(TEST_CFLAGS)
lib_tests_test_serialize_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_msgparse_CFLAGS		= $(TEST_CFLAGS)
lib_tests_test_msgparse_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_dnscache_CFLAGS		= $(TEST_CFLAGS)
lib_tests_test_dnscache_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_findcrlf_CFLAGS		= $(TEST_CFLAGS)
lib_tests_test_findcrlf_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_ringbuffer_CFLAGS	= $(TEST_CFLAGS)
lib_tests_test_ringbuffer_LDADD	= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_hostid_CFLAGS		= $(TEST_CFLAGS)
lib_tests_test_hostid_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_tests_test_zone_LDADD		= \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)
lib_tests_test_zone_CFLAGS		= $(TEST_CFLAGS)

lib_tests_test_thread_wakeup_CFLAGS	= $(TEST_CFLAGS)
lib_tests_test_thread_wakeup_LDADD	= $(TEST_LDADD)


EXTRA_DIST += \
	lib/tests/testdata-lexer/include-test/bar.conf			\
	lib/tests/testdata-lexer/include-test/baz.conf 			\
	lib/tests/testdata-lexer/include-test/foo.conf 			\
	lib/tests/testdata-lexer/include-test/invalid~include~file.conf
