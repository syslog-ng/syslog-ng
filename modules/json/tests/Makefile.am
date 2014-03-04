moduledir = @moduledir@
export top_srcdir

if !IS_WINDOWS
if ENABLE_JSON

AM_CPPFLAGS = -I$(top_srcdir)/libtest -I$(top_srcdir)/lib -I../../lib -I$(top_srcdir)/modules/json

AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen $(top_builddir)/modules/json/libjson-plugin.la

LDADD = $(JSON_LIBS) $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_format_json test_json_parser test_dot_notation

test_format_json_SOURCES = test_format_json.c
test_format_json_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

test_json_parser_SOURCES = test_json_parser.c
test_json_parser_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

test_dot_notation_SOURCES = test_dot_notation.c
test_dot_notation_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

TESTS = $(check_PROGRAMS)

endif
endif
