module_LTLIBRARIES      += modules/throttle-filter/libthrottle-filter.la
modules_throttle_filter_libthrottle_filter_la_SOURCES = \
  modules/throttle-filter/throttle-filter-grammar.y       \
  modules/throttle-filter/throttle-filter-parser.c        \
  modules/throttle-filter/throttle-filter-parser.h        \
  modules/throttle-filter/throttle-filter-plugin.c

BUILT_SOURCES       +=      \
  modules/throttle-filter/throttle-filter-grammar.y       \
  modules/throttle-filter/throttle-filter-grammar.c       \
  modules/throttle-filter/throttle-filter-grammar.h

EXTRA_DIST        +=      \
  modules/throttle-filter/throttle-filter-grammar.ym \
  modules/throttle-filter/CMakeLists.txt

modules_throttle_filter_libthrottle_filter_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/throttle-filter        \
  -I$(top_builddir)/modules/throttle-filter
modules_throttle_filter_libthrottle_filter_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_throttle_filter_libthrottle_filter_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_throttle_filter_libthrottle_filter_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/throttle-filter modules/throttle-filter/ mod-throttle-filter: modules/throttle-filter/libthrottle-filter.la
.PHONY: modules/throttle-filter/ mod-throttle-filter
