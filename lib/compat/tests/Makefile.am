lib_compat_tests_TESTS		 = \
	lib/compat/tests/test_strtok_r \
	lib/compat/tests/test_openssl_pkcs12

EXTRA_DIST +=	\
	lib/compat/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_compat_tests_TESTS}

lib_compat_tests_test_strtok_r_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/compat/tests
lib_compat_tests_test_strtok_r_LDADD	 = $(TEST_LDADD)
lib_compat_tests_test_strtok_r_SOURCES = \
	lib/compat/tests/test_strtok_r.c

lib_compat_tests_test_openssl_pkcs12_CFLAGS = $(TEST_CFLAGS)
lib_compat_tests_test_openssl_pkcs12_LDADD = $(TEST_LDADD)
lib_compat_tests_test_openssl_pkcs12_SOURCES = \
	lib/compat/tests/test_openssl_pkcs12.c
