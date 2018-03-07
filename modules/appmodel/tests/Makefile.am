modules_appmodel_tests_TESTS			= \
	modules/appmodel/tests/test_appmodel	\
	modules/appmodel/tests/test_appmodel_context	\
	modules/appmodel/tests/test_app_parser_generator	\
	modules/appmodel/tests/test_application

check_PROGRAMS					+=	\
	${modules_appmodel_tests_TESTS}

modules_appmodel_tests_test_appmodel_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/appmodel
modules_appmodel_tests_test_appmodel_LDADD	=	\
	$(TEST_LDADD)
modules_appmodel_tests_test_appmodel_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/appmodel/libappmodel.la
modules_appmodel_tests_test_appmodel_DEPENDENCIES =      \
        $(top_builddir)/modules/appmodel/libappmodel.la

modules_appmodel_tests_test_appmodel_context_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/appmodel
modules_appmodel_tests_test_appmodel_context_LDADD	=	\
	$(TEST_LDADD)
modules_appmodel_tests_test_appmodel_context_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/appmodel/libappmodel.la
modules_appmodel_tests_test_appmodel_context_DEPENDENCIES =      \
        $(top_builddir)/modules/appmodel/libappmodel.la

modules_appmodel_tests_test_app_parser_generator_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/appmodel
modules_appmodel_tests_test_app_parser_generator_LDADD	=	\
	$(TEST_LDADD)
modules_appmodel_tests_test_app_parser_generator_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/appmodel/libappmodel.la
modules_appmodel_tests_test_app_parser_generator_DEPENDENCIES =      \
        $(top_builddir)/modules/appmodel/libappmodel.la

modules_appmodel_tests_test_application_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/appmodel
modules_appmodel_tests_test_application_LDADD	=	\
	$(TEST_LDADD)
modules_appmodel_tests_test_application_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/appmodel/libappmodel.la
modules_appmodel_tests_test_application_DEPENDENCIES =      \
        $(top_builddir)/modules/appmodel/libappmodel.la
