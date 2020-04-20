if HAVE_GETRANDOM

noinst_LTLIBRARIES += modules/examples/sources/libthreaded-random-generator.la

modules_examples_sources_libthreaded_random_generator_la_SOURCES = \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar.y \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-parser.c \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-parser.h \
  modules/examples/sources/threaded-random-generator/threaded-random-generator.c \
  modules/examples/sources/threaded-random-generator/threaded-random-generator.h

modules_examples_sources_libthreaded_random_generator_la_CPPFLAGS  = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/examples/sources/threaded-random-generator \
  -I$(top_builddir)/modules/examples/sources/threaded-random-generator

modules_examples_sources_libthreaded_random_generator_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_examples_sources_libthreaded_random_generator_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_examples_sources_libthreaded_random_generator_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

endif

BUILT_SOURCES += \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar.y \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar.c \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar.h

EXTRA_DIST += \
  modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar.ym \
  modules/examples/sources/threaded-random-generator/CMakeLists.txt
