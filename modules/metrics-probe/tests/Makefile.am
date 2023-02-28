modules_metrics_probe_tests_TESTS			=	\
	modules/metrics-probe/tests/test_metrics_probe

check_PROGRAMS					+=	\
	${modules_metrics_probe_tests_TESTS}

EXTRA_DIST += modules/metrics-probe/tests/CMakeLists.txt

modules_metrics_probe_tests_test_metrics_probe_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/metrics-probe
modules_metrics_probe_tests_test_metrics_probe_LDADD	=	\
	$(TEST_LDADD)					\
	-dlpreopen $(top_builddir)/modules/metrics-probe/libmetrics-probe.la
