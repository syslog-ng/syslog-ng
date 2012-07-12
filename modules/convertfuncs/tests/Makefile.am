AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/convertfuncs -I.. -I${top_srcdir}/tests/unit @CFLAGS_NOWARN_POINTER_SIGN@
if IS_WINDOWS
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la
else
AM_LDFLAGS = ../../syslogformat/libsyslogformat.la
endif
LDADD = ../libconvertfuncs.la $(top_builddir)/lib/libsyslog-ng.la @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@

check_PROGRAMS = test_replace test_format_snare

TESTUTILS=$(top_srcdir)/tests/unit/testutils.c $(top_srcdir)/tests/unit/testutils.h

test_replace_SOURCES = test_replace.c $(TESTUTILS)
test_format_snare_SOURCES = test_format_snare.c $(TESTUTILS)

TESTS = $(check_PROGRAMS)
