lib_control_tests_TESTS		 = \
	lib/control/tests/test_control_cmds	\
	lib/control/tests/test_control_connection

EXTRA_DIST +=	\
	lib/control/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_control_tests_TESTS}

lib_control_tests_test_control_cmds_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/control/tests
lib_control_tests_test_control_cmds_LDADD	= $(TEST_LDADD) \
	 -dlpreopen $(top_builddir)/lib/secret-storage/libsecret-storage.la
lib_control_tests_test_control_cmds_SOURCES	= 		\
	lib/control/tests/test_control_cmds.c

lib_control_tests_test_control_connection_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/control/tests
lib_control_tests_test_control_connection_LDADD	= $(TEST_LDADD)
lib_control_tests_test_control_connection_SOURCES	= 		\
	lib/control/tests/test_control_connection.c
