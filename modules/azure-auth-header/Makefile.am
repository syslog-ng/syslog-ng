module_LTLIBRARIES      += modules/azure-auth-header/libazure-auth-header.la
modules_azure_auth_header_libazure_auth_header_la_SOURCES = \
  modules/azure-auth-header/azure-auth-header-grammar.y       \
  modules/azure-auth-header/azure-auth-header-parser.c        \
  modules/azure-auth-header/azure-auth-header-parser.h        \
  modules/azure-auth-header/azure-auth-header-plugin.c        \
  modules/azure-auth-header/azure-auth-header.c               \
  modules/azure-auth-header/azure-auth-header.h               

BUILT_SOURCES       +=      \
  modules/azure-auth-header/azure-auth-header-grammar.y       \
  modules/azure-auth-header/azure-auth-header-grammar.c       \
  modules/azure-auth-header/azure-auth-header-grammar.h

EXTRA_DIST        +=      \
  modules/azure-auth-header/azure-auth-header-grammar.ym \
  modules/azure-auth-header/CMakeLists.txt

modules_azure_auth_header_libazure_auth_header_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/azure-auth-header -I$(top_builddir)/modules/azure-auth-header
modules_azure_auth_header_libazure_auth_header_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_azure_auth_header_libazure_auth_header_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_azure_auth_header_libazure_auth_header_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

modules/azure-auth-header modules/azure-auth-header/ mod-azure-auth-header: modules/azure-auth-header/libazure-auth-header.la

.PHONY: modules/azure-auth-header/ mod-azure-auth-header
