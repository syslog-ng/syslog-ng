modules_confgen_tests_TESTS		= \
	modules/confgen/tests/test_confgen

check_PROGRAMS		+= ${modules_confgen_tests_TESTS}

modules_confgen_tests_test_confgen_CFLAGS	=	\
	$(TEST_CFLAGS)
modules_confgen_tests_test_confgen_LDADD	=	\
	$(TEST_LDADD) -dlpreopen $(top_builddir)/modules/confgen/libconfgen.la

EXTRA_DIST += modules/confgen/tests/confgentest.sh \
              modules/confgen/tests/CMakeLists.txt
