module_LTLIBRARIES      += modules/stardate/libstardate.la
modules_stardate_libstardate_la_SOURCES = \
  modules/stardate/stardate-plugin.c

EXTRA_DIST        +=      \
  modules/stardate/stardate.c	\
  modules/stardate/CMakeLists.txt

modules_stardate_libstardate_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/stardate        \
  -I$(top_builddir)/modules/stardate
modules_stardate_libstardate_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_stardate_libstardate_la_LDFLAGS = $(MODULE_LDFLAGS) -lm
modules_stardate_libstardate_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/stardate modules/stardate/ mod-stardate: modules/stardate/libstardate.la
.PHONY: modules/stardate/ mod-stardate

include modules/stardate/tests/Makefile.am
