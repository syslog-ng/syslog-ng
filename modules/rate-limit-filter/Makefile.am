module_LTLIBRARIES      += modules/rate-limit-filter/librate-limit-filter.la
modules_rate_limit_filter_librate_limit_filter_la_SOURCES = \
  modules/rate-limit-filter/rate-limit-grammar.y       \
  modules/rate-limit-filter/rate-limit-parser.c        \
  modules/rate-limit-filter/rate-limit-parser.h        \
  modules/rate-limit-filter/rate-limit-plugin.c \
  modules/rate-limit-filter/rate-limit.h \
  modules/rate-limit-filter/rate-limit.c

BUILT_SOURCES       +=      \
  modules/rate-limit-filter/rate-limit-grammar.y       \
  modules/rate-limit-filter/rate-limit-grammar.c       \
  modules/rate-limit-filter/rate-limit-grammar.h

EXTRA_DIST        +=      \
  modules/rate-limit-filter/rate-limit-grammar.ym \
  modules/rate-limit-filter/CMakeLists.txt

modules_rate_limit_filter_librate_limit_filter_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/rate-limit-filter        \
  -I$(top_builddir)/modules/rate-limit-filter
modules_rate_limit_filter_librate_limit_filter_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_rate_limit_filter_librate_limit_filter_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_rate_limit_filter_librate_limit_filter_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/rate-limit-filter modules/rate-limit-filter/ mod-rate-limit-filter: modules/rate-limit-filter/librate-limit-filter.la
.PHONY: modules/rate-limit-filter/ mod-rate-limit-filter
