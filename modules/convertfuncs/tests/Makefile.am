AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/convertfuncs -I.. -I${top_srcdir}/libtest @CFLAGS_NOWARN_POINTER_SIGN@
if IS_WINDOWS
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la
else
AM_LDFLAGS = ../../syslogformat/libsyslogformat.la
endif
LDADD = ../libconvertfuncs.la $(top_builddir)/lib/libsyslog-ng.la @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@ $(top_builddir)/libtest/libsyslog-ng-test.a

check_PROGRAMS = test_replace test_format_snare test_lowercase test_strcut

test_replace_SOURCES = test_replace.c
test_format_snare_SOURCES = test_format_snare.c
test_lowercase_SOURCES = test_lowercase.c
test_strcut_SOURCES = test_strcut.c

TESTS = $(check_PROGRAMS)
