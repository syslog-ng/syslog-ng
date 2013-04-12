AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/libtest -I../../../libtest -I$(top_srcdir)/modules/afsocket -I..
AM_LDFLAGS = -dlpreopen ../../afsocket/libafsocket.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test-transport-mapper test-transport-mapper-inet test-transport-mapper-unix

TRANSPORT_MAPPER_LIB = transport-mapper-lib.c transport-mapper-lib.h

test_transport_mapper_SOURCES = test-transport-mapper.c $(TRANSPORT_MAPPER_LIB)
test_transport_mapper_inet_SOURCES = test-transport-mapper-inet.c $(TRANSPORT_MAPPER_LIB)
test_transport_mapper_unix_SOURCES = test-transport-mapper-unix.c $(TRANSPORT_MAPPER_LIB)

TESTS = $(check_PROGRAMS)
