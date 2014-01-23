AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/convertfuncs -I.. -I${top_srcdir}/libtest @CFLAGS_NOWARN_POINTER_SIGN@
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen ../libconvertfuncs.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a  @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@

check_PROGRAMS = test_replace test_format_snare test_lowercase test_cut test_ipv4toint test_formatwelf

test_replace_SOURCES = test_replace.c
test_format_snare_SOURCES = test_format_snare.c
test_lowercase_SOURCES = test_lowercase.c
test_cut_SOURCES = test_cut.c
test_ipv4toint_SOURCES = test_ipv4toint.c
test_formatwelf_SOURCES = test_formatwelf.c

TESTS = $(check_PROGRAMS)
