module_LTLIBRARIES				+= \
  modules/graphite/libgraphite.la

EXTRA_DIST += modules/graphite/CMakeLists.txt

modules_graphite_libgraphite_la_SOURCES		 = \
  modules/graphite/graphite-plugin.c		   \
  modules/graphite/graphite-output.h		   \
  modules/graphite/graphite-output.c

modules_graphite_libgraphite_la_CPPFLAGS		 = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/graphite		   \
  -I$(top_builddir)/modules/graphite
modules_graphite_libgraphite_la_LIBADD		 = \
  $(MODULE_DEPS_LIBS)
modules_graphite_libgraphite_la_LDFLAGS		 = \
  $(MODULE_LDFLAGS)
modules_graphite_libgraphite_la_DEPENDENCIES	 = \
  $(MODULE_DEPS_LIBS)

modules/graphite mod-graphite: modules/graphite/libgraphite.la

.PHONY: modules/graphite mod-graphite

include modules/graphite/tests/Makefile.am
