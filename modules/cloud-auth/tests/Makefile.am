if ENABLE_CLOUD_AUTH

modules_cloud_auth_tests_TESTS = modules/cloud-auth/tests/test_cloud_auth
check_PROGRAMS += $(modules_cloud_auth_tests_TESTS)

modules_cloud_auth_tests_test_cloud_auth_SOURCES = \
  modules/cloud-auth/tests/test-cloud-auth.cpp

modules_cloud_auth_tests_test_cloud_auth_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  $(LIBCURL_CFLAGS) \
  -I$(top_srcdir)/modules/cloud-auth \
  -I$(top_builddir)/modules/cloud-auth \
  -isystem $(top_srcdir)/modules/cloud-auth/jwt-cpp/include \
  -isystem $(top_builddir)/modules/cloud-auth/jwt-cpp/include

modules_cloud_auth_tests_test_cloud_auth_LDADD = \
  $(top_builddir)/modules/cloud-auth/libcloud_auth_cpp.la \
  $(TEST_LDADD) \
  $(LIBCURL_LIBS)

EXTRA_modules_cloud_auth_tests_test_cloud_auth_DEPENDENCIES = \
  $(top_builddir)/modules/cloud-auth/libcloud_auth_cpp.la

endif

EXTRA_DIST += \
  modules/cloud-auth/tests/CMakeLists.txt \
  modules/cloud-auth/tests/fixtures/README.md \
  modules/cloud-auth/tests/fixtures/test-only-certificate.pem \
  modules/cloud-auth/tests/fixtures/test-only-private-key.pem \
  modules/cloud-auth/tests/fixtures/test-only-encrypted-private-key.pem
