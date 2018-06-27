EXTRA_DIST += modules/afstomp/tests/CMakeLists.txt

if ENABLE_STOMP
modules_afstomp_tests_test_stomp_proto_CFLAGS = \
    $(TEST_CFLAGS) \
    -I$(top_srcdir)/modules/afstomp

modules_afstomp_tests_test_stomp_proto_LDADD = \
    $(TEST_LDADD) $(MODULE_DEPS_LIBS)

modules_afstomp_tests_test_stomp_proto_LDFLAGS = \
    -dlpreopen $(top_builddir)/modules/afstomp/libafstomp.la

modules_afstomp_tests_TESTS =   \
    modules/afstomp/tests/test_stomp_proto

check_PROGRAMS +=   \
    $(modules_afstomp_tests_TESTS)
endif
