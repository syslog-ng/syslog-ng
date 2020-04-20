lib_template_tests_TESTS	=		 \
	lib/template/tests/test_template_compile 	\
	lib/template/tests/test_template_on_error 	\
	lib/template/tests/test_template	 	\
	lib/template/tests/test_template_speed		\
	lib/template/tests/test_macro

check_PROGRAMS		+= ${lib_template_tests_TESTS}

lib_template_tests_test_template_compile_CFLAGS	=	\
	$(TEST_CFLAGS)
lib_template_tests_test_template_compile_LDADD	=	\
	$(TEST_LDADD)

lib_template_tests_test_template_on_error_CFLAGS	= \
	$(TEST_CFLAGS)
lib_template_tests_test_template_on_error_LDADD		= \
	$(TEST_LDADD)

lib_template_tests_test_template_CFLAGS		= $(TEST_CFLAGS)
lib_template_tests_test_template_LDADD		= \
	$(TEST_LDADD) 			\
	$(PREOPEN_SYSLOGFORMAT)		\
	$(PREOPEN_BASICFUNCS)


lib_template_tests_test_template_speed_CFLAGS = $(TEST_CFLAGS)
lib_template_tests_test_template_speed_LDADD = \
	$(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT) $(PREOPEN_BASICFUNCS)

lib_template_tests_test_macro_CFLAGS = $(TEST_CFLAGS)
lib_template_tests_test_macro_LDADD = \
	$(TEST_LDADD)
