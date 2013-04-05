AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/upgrade-tool -I$(top_srcdir)/modules/agent-config -I.. -I${top_srcdir}/libtest @CFLAGS_NOWARN_POINTER_SIGN@
LDADD = @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@ $(top_builddir)/libtest/libsyslog-ng-test.a $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/modules/agent-config/libagent-config.la

check_PROGRAMS = test_upgrade_state

test_upgrade_state_SOURCES = test_upgrade_state.c

TESTS = $(check_PROGRAMS)
