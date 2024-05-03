if ENABLE_CLOUD_AUTH

noinst_LTLIBRARIES += modules/cloud-auth/libcloud_auth_cpp.la

modules_cloud_auth_libcloud_auth_cpp_la_SOURCES = \
  modules/cloud-auth/cloud-auth.hpp \
  modules/cloud-auth/cloud-auth.cpp \
  modules/cloud-auth/google-auth.hpp \
  modules/cloud-auth/google-auth.cpp

modules_cloud_auth_libcloud_auth_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(LIBCURL_CFLAGS) \
  -I$(top_srcdir)/modules/cloud-auth \
  -I$(top_builddir)/modules/cloud-auth \
  -isystem $(top_srcdir)/modules/cloud-auth/jwt-cpp/include \
  -isystem $(top_builddir)/modules/cloud-auth/jwt-cpp/include

modules_cloud_auth_libcloud_auth_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBCURL_LIBS)
modules_cloud_auth_libcloud_auth_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_cloud_auth_libcloud_auth_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)


module_LTLIBRARIES += modules/cloud-auth/libcloud_auth.la
modules_cloud_auth_libcloud_auth_la_SOURCES = \
  modules/cloud-auth/cloud-auth-grammar.y \
  modules/cloud-auth/cloud-auth-parser.c \
  modules/cloud-auth/cloud-auth-parser.h \
  modules/cloud-auth/cloud-auth-plugin.c \
  modules/cloud-auth/cloud-auth.h \
  modules/cloud-auth/cloud-auth.c \
  modules/cloud-auth/google-auth.h

modules_cloud_auth_libcloud_auth_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/cloud-auth \
  -I$(top_builddir)/modules/cloud-auth

modules_cloud_auth_libcloud_auth_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/cloud-auth/libcloud_auth_cpp.la

nodist_EXTRA_modules_cloud_auth_libcloud_auth_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_cloud_auth_libcloud_auth_la_LDFLAGS = \
  $(MODULE_LDFLAGS)

EXTRA_modules_cloud_auth_libcloud_auth_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/cloud-auth/libcloud_auth_cpp.la

modules/cloud-auth modules/cloud-auth/ mod-cloud-auth: modules/cloud-auth/libcloud_auth.la
else
modules/cloud-auth modules/cloud-auth/ mod-cloud-auth:
endif

BUILT_SOURCES += \
  modules/cloud-auth/cloud-auth-grammar.y \
  modules/cloud-auth/cloud-auth-grammar.c \
  modules/cloud-auth/cloud-auth-grammar.h

EXTRA_DIST += \
  modules/cloud-auth/cloud-auth-grammar.ym \
  modules/cloud-auth/CMakeLists.txt \
  \
  modules/cloud-auth/jwt-cpp/LICENSE \
  modules/cloud-auth/jwt-cpp/include/nlohmann/json.hpp \
  modules/cloud-auth/jwt-cpp/include/picojson/picojson.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/jwt.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/nlohmann-json/defaults.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/nlohmann-json/traits.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/boost-json/defaults.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/boost-json/traits.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/danielaparker-jsoncons/defaults.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/danielaparker-jsoncons/traits.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/kazuho-picojson/defaults.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/kazuho-picojson/traits.h \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/traits/defaults.h.mustache \
  modules/cloud-auth/jwt-cpp/include/jwt-cpp/base.h

.PHONY: modules/cloud-auth/ mod-cloud-auth
