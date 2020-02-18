modules_secure_logging_tests_TESTS				=	\
	modules/secure-logging/tests/test_secure_logging
check_PROGRAMS						+=	\
	${modules_secure_logging_tests_TESTS}

EXTRA_DIST += modules/secure-logging/tests/CMakeLists.txt

modules_secure_logging_tests_test_secure_logging_CFLAGS	=	\
	$(TEST_CFLAGS)                                          \
        -I${top_srcdir}/modules/secure-logging                  \
        -I${top_builddir}/modules/secure-logging
modules_secure_logging_tests_test_secure_logging_LDADD	=	\
	$(TEST_LDADD)						\
	-dlpreopen ${top_builddir}/modules/secure-logging/libsecure-logging.la \
	-dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la	
