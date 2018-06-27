modules_cryptofuncs_tests_TESTS				=	\
	modules/cryptofuncs/tests/test_cryptofuncs
check_PROGRAMS						+=	\
	${modules_cryptofuncs_tests_TESTS}

EXTRA_DIST += modules/cryptofuncs/tests/CMakeLists.txt

modules_cryptofuncs_tests_test_cryptofuncs_CFLAGS	=	\
	$(TEST_CFLAGS)
modules_cryptofuncs_tests_test_cryptofuncs_LDADD	=	\
	$(TEST_LDADD)						\
	-dlpreopen ${top_builddir}/modules/cryptofuncs/libcryptofuncs.la
