if ENABLE_CLOUD_AUTH

noinst_LTLIBRARIES += modules/cloud-auth/libcloud_auth_cpp.la

modules_cloud_auth_libcloud_auth_cpp_la_SOURCES = \
  modules/cloud-auth/cloud-auth.hpp \
  modules/cloud-auth/cloud-auth.cpp

modules_cloud_auth_libcloud_auth_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  -I$(top_srcdir)/modules/cloud-auth \
  -I$(top_builddir)/modules/cloud-auth

modules_cloud_auth_libcloud_auth_cpp_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_cloud_auth_libcloud_auth_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_cloud_auth_libcloud_auth_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)


module_LTLIBRARIES += modules/cloud-auth/libcloud_auth.la
modules_cloud_auth_libcloud_auth_la_SOURCES = \
  modules/cloud-auth/cloud-auth-grammar.y \
  modules/cloud-auth/cloud-auth-parser.c \
  modules/cloud-auth/cloud-auth-parser.h \
  modules/cloud-auth/cloud-auth-plugin.c \
  modules/cloud-auth/cloud-auth.h \
  modules/cloud-auth/cloud-auth.c

modules_cloud_auth_libcloud_auth_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/cloud-auth \
  -I$(top_builddir)/modules/cloud-auth

modules_cloud_auth_libcloud_auth_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/cloud-auth/libcloud_auth_cpp.la

modules_cloud_auth_libcloud_auth_la_LDFLAGS = \
  $(MODULE_LDFLAGS)

modules_cloud_auth_libcloud_auth_la_DEPENDENCIES = \
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
  modules/cloud-auth/CMakeLists.txt

.PHONY: modules/cloud-auth/ mod-cloud-auth
