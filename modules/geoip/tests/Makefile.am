if ENABLE_GEOIP
modules_geoip_tests_TESTS		= \
	modules/geoip/tests/test_geoip_parser

check_PROGRAMS				+= ${modules_geoip_tests_TESTS}

modules_geoip_tests_test_geoip_parser_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/geoip
modules_geoip_tests_test_geoip_parser_LDADD	= $(TEST_LDADD)
modules_geoip_tests_test_geoip_parser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/geoip/libgeoip-plugin.la
modules_geoip_tests_test_geoip_parser_DEPENDENCIES = $(top_builddir)/modules/geoip/libgeoip-plugin.la
endif

EXTRA_DIST += modules/geoip/tests/CMakeLists.txt
