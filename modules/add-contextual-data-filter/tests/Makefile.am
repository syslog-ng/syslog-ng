if ENABLE_CRITERION
modules_add_contextual_data_selector_filter_tests_TESTS = \
        modules/add-contextual-data-selector-filter/tests/test_selector_filter

check_PROGRAMS        += \
  ${modules_add_contextual_data_selector_filter_tests_TESTS}

modules_add_contextual_data_selector_filter_tests_test_selector_filter_CFLAGS   =       \
        $(TEST_CFLAGS) $(PE_MODULES_CFLAGS) $(SYSLOG_NG_ADD_CTX_DATA_CFLAGS) -I$(top_srcdir)/modules/add-contextual-data-selector-filter
modules_add_contextual_data_selector_filter_tests_test_selector_filter_LDADD    =       \
        $(TEST_LDADD) $(PE_MODULES_LIBS) $(SYSLOG_NG_ADD_CTX_DATA_LIBS)
modules_add_contextual_data_selector_filter_tests_test_selector_filter_LDFLAGS  =       \
        -dlpreopen $(top_builddir)/modules/add-contextual-data-selector-filter/libadd-contextual-data-selector-filter.la

endif
