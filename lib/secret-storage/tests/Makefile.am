lib_secret_storage_tests_TESTS			= \
	lib/secret-storage/tests/test_secret_storage

check_PROGRAMS					+= ${lib_secret_storage_tests_TESTS}

lib_secret_storage_tests_test_secret_storage_CFLAGS	= $(TEST_CFLAGS) \
	-I$(top_srcdir)/lib/secret-storage

lib_secret_storage_tests_test_secret_storage_LDADD	= \
	$(TEST_LDADD) \
	$(PREOPEN_SYSLOGFORMAT) -dlpreopen $(top_builddir)/lib/secret-storage/libsecret-storage.la
