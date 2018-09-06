include modules/examples/sources/msg-generator/Makefile.am

EXAMPLE_PLUGINS = \
  $(top_builddir)/modules/examples/sources/libmsg-generator.la

if HAVE_GETRANDOM
include modules/examples/sources/threaded-random-generator/Makefile.am
EXAMPLE_PLUGINS += $(top_builddir)/modules/examples/sources/libthreaded-random-generator.la
endif


module_LTLIBRARIES += modules/examples/libexamples.la

modules_examples_libexamples_la_SOURCES = \
  modules/examples/example-plugins.c

modules_examples_libexamples_la_LIBADD = $(MODULE_DEPS_LIBS) $(EXAMPLE_PLUGINS)
modules_examples_libexamples_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(EXAMPLE_PLUGINS)
modules_examples_libexamples_la_LDFLAGS = $(MODULE_LDFLAGS)

EXTRA_DIST += modules/examples/CMakeLists.txt

modules/examples modules/examples/ mod-examples: modules/examples/libexamples.la

.PHONY: modules/examples/ mod-examples
