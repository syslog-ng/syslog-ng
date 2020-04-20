lib_str_repr_tests_TESTS		= \
	lib/str-repr/tests/test_decode		\
	lib/str-repr/tests/test_encode

EXTRA_DIST += lib/str-repr/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_str_repr_tests_TESTS}

lib_str_repr_tests_test_decode_CFLAGS	= $(TEST_CFLAGS)
lib_str_repr_tests_test_decode_LDADD	=	\
	$(TEST_LDADD)

lib_str_repr_tests_test_encode_CFLAGS	= $(TEST_CFLAGS)
lib_str_repr_tests_test_encode_LDADD	=	\
	$(TEST_LDADD)
