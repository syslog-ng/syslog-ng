lib_control_tests_TESTS		 = \
	lib/control/tests/test_control_cmds

EXTRA_DIST +=	\
	lib/control/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_control_tests_TESTS}

lib_control_tests_test_control_cmds_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/control/tests
lib_control_tests_test_control_cmds_LDADD	= $(TEST_LDADD) \
	 -dlpreopen $(top_builddir)/lib/secret-storage/libsecret-storage.la
lib_control_tests_test_control_cmds_SOURCES	= 		\
	lib/control/tests/test_control_cmds.c		\
	lib/control/tests/control-server-dummy.c	\
	lib/control/tests/control-server-dummy.h
