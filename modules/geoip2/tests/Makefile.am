if ENABLE_GEOIP2
modules_geoip2_tests_TESTS		= \
	modules/geoip2/tests/test_geoip_parser

check_PROGRAMS				+= ${modules_geoip2_tests_TESTS}

modules_geoip2_tests_test_geoip_parser_CFLAGS	= $(TEST_CFLAGS) \
	-I$(top_srcdir)/modules/geoip2 -DTOP_SRCDIR="\"$(abs_topsrcdir)\""
modules_geoip2_tests_test_geoip_parser_LDADD	= $(TEST_LDADD)
modules_geoip2_tests_test_geoip_parser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/geoip2/libgeoip2-plugin.la
modules_geoip2_tests_test_geoip_parser_DEPENDENCIES = $(top_builddir)/modules/geoip2/libgeoip2-plugin.la
endif
