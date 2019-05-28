lib_tests_TESTS		= \
	lib/tests/test_cfg_lexer_subst	\
	lib/tests/test_lexer_block	\
	lib/tests/test_cfg_tree		\
	lib/tests/test_type_hints	\
	lib/tests/test_parse_number	\
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
	lib/tests/test_logqueue

EXTRA_DIST += lib/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_tests_TESTS}

if ENABLE_TESTING
noinst_PROGRAMS 	= \
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

lib_tests_test_type_hints_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_type_hints_LDADD	=	\
	$(TEST_LDADD)

lib_tests_test_parse_number_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_tests_test_parse_number_LDADD	=	\
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

CLEANFILES				+= \
	test_values.persist		   \
	test_values.persist-		   \
	test_run_id.persist		   \
	test_run_id.persist-

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

EXTRA_DIST += \
	lib/tests/testdata-lexer/include-test/bar.conf			\
	lib/tests/testdata-lexer/include-test/baz.conf 			\
	lib/tests/testdata-lexer/include-test/foo.conf 			\
	lib/tests/testdata-lexer/include-test/invalid~include~file.conf
