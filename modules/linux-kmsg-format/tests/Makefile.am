modules_linux_kmsg_format_tests_TESTS				=	\
	modules/linux-kmsg-format/tests/test_linux_format_kmsg

EXTRA_DIST += modules/linux-kmsg-format/tests/CMakeLists.txt

check_PROGRAMS							+=	\
	${modules_linux_kmsg_format_tests_TESTS}

modules_linux_kmsg_format_tests_test_linux_format_kmsg_CFLAGS	=	\
	$(TEST_CFLAGS)
modules_linux_kmsg_format_tests_test_linux_format_kmsg_LDADD	=	\
	$(TEST_LDADD)
modules_linux_kmsg_format_tests_test_linux_format_kmsg_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)						\
	-dlpreopen ${top_builddir}/modules/linux-kmsg-format/liblinux-kmsg-format.la
