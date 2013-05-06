modules_linux_kmsg_format_tests_TESTS				=	\
	modules/linux-kmsg-format/tests/test_linux_format_kmsg

check_PROGRAMS							+=	\
	${modules_linux_kmsg_format_tests_TESTS}

modules_linux_kmsg_format_tests_test_linux_format_kmsg_CFLAGS	=	\
	$(TEST_CFLAGS)
modules_linux_kmsg_format_tests_test_linux_format_kmsg_LDADD	=	\
	$(TEST_LDADD)
modules_linux_kmsg_format_tests_test_linux_format_kmsg_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen ${top_builddir}/modules/linux-kmsg-format/liblinux-kmsg-format.la
