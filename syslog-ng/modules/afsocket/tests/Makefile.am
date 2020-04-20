TEST_TRANSPORT_MAPPER_CFLAGS = $(TEST_CFLAGS)

EXTRA_DIST += modules/afsocket/tests/CMakeLists.txt

TRANSPORT_MAPPER_LIB = 					\
	modules/afsocket/tests/transport-mapper-lib.c	\
	modules/afsocket/tests/transport-mapper-lib.h

modules_afsocket_tests_TESTS			=		\
	modules/afsocket/tests/test-transport-mapper		\
	modules/afsocket/tests/test-transport-mapper-inet	\
	modules/afsocket/tests/test-transport-mapper-unix

check_PROGRAMS					+=	\
	$(modules_afsocket_tests_TESTS)

modules_afsocket_tests_test_transport_mapper_CFLAGS = 	\
	$(TEST_TRANSPORT_MAPPER_CFLAGS)			\
	-I$(top_srcdir)/modules/afsocket

modules_afsocket_tests_test_transport_mapper_LDADD = 	\
	$(TEST_LDADD)

modules_afsocket_tests_test_transport_mapper_LDFLAGS =	\
	-dlpreopen $(top_builddir)/modules/afsocket/libafsocket.la

modules_afsocket_tests_test_transport_mapper_SOURCES = 	\
	modules/afsocket/tests/test-transport-mapper.c	\
	$(TRANSPORT_MAPPER_LIB)

modules_afsocket_tests_test_transport_mapper_inet_CFLAGS = 	\
	$(TEST_TRANSPORT_MAPPER_CFLAGS)				\
	-I$(top_srcdir)/modules/afsocket

modules_afsocket_tests_test_transport_mapper_inet_LDADD = 	\
	$(TEST_LDADD)

modules_afsocket_tests_test_transport_mapper_inet_LDFLAGS =	\
	-dlpreopen $(top_builddir)/modules/afsocket/libafsocket.la

modules_afsocket_tests_test_transport_mapper_inet_SOURCES = 	\
	modules/afsocket/tests/test-transport-mapper-inet.c	\
	$(TRANSPORT_MAPPER_LIB)

modules_afsocket_tests_test_transport_mapper_unix_CFLAGS = 	\
	$(TEST_TRANSPORT_MAPPER_CFLAGS)				\
	-I$(top_srcdir)/modules/afsocket

modules_afsocket_tests_test_transport_mapper_unix_LDADD = 	\
	$(TEST_LDADD)

modules_afsocket_tests_test_transport_mapper_unix_LDFLAGS =	\
	-dlpreopen $(top_builddir)/modules/afsocket/libafsocket.la

modules_afsocket_tests_test_transport_mapper_unix_SOURCES = 	\
	modules/afsocket/tests/test-transport-mapper-unix.c	\
	$(TRANSPORT_MAPPER_LIB)
