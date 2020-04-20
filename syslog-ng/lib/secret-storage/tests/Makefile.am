lib_secret_storage_tests_TESTS			= \
	lib/secret-storage/tests/test_secret_storage \
	lib/secret-storage/tests/test_nondumpable_allocator

EXTRA_DIST += lib/secret-storage/tests/CMakeLists.txt

check_PROGRAMS					+= ${lib_secret_storage_tests_TESTS}

lib_secret_storage_tests_test_secret_storage_CFLAGS	= $(TEST_CFLAGS) \
	-I$(top_srcdir)/lib/secret-storage

lib_secret_storage_tests_test_secret_storage_LDADD	= \
	$(TEST_LDADD) \
	$(PREOPEN_SYSLOGFORMAT) -dlpreopen $(top_builddir)/lib/secret-storage/libsecret-storage.la

lib_secret_storage_tests_test_nondumpable_allocator_CFLAGS	= $(TEST_CFLAGS) \
	-I$(top_srcdir)/lib/secret-storage

lib_secret_storage_tests_test_nondumpable_allocator_LDADD	= \
	$(TEST_LDADD) \
	$(PREOPEN_SYSLOGFORMAT) -dlpreopen $(top_builddir)/lib/secret-storage/libsecret-storage.la
