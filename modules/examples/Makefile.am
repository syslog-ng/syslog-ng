if ENABLE_EXAMPLE_MODULES

include modules/examples/sources/msg-generator/Makefile.am
include modules/examples/sources/threaded-diskq-source/Makefile.am
include modules/examples/sources/threaded-random-generator/Makefile.am
include modules/examples/inner-destinations/http-test-slots/Makefile.am
include modules/examples/inner-destinations/tls-test-validation/Makefile.am
include modules/examples/destinations/example_destination/Makefile.am
include modules/examples/sources/random-choice-generator/Makefile.am
include modules/examples/filterx/example-filterx-func/Makefile.am

EXAMPLE_PLUGINS = \
  $(top_builddir)/modules/examples/sources/libmsg-generator.la \
  $(top_builddir)/modules/examples/sources/libthreaded-diskq-source.la \
  $(top_builddir)/modules/examples/inner-destinations/http-test-slots/libhttp-test-slots.la \
  $(top_builddir)/modules/examples/inner-destinations/tls-test-validation/libtls-test-validation.la \
  $(top_builddir)/modules/examples/destinations/example_destination/libexample_destination.la \
  $(top_builddir)/modules/examples/filterx/example-filterx-func/libexample-filterx-func.la

if HAVE_GETRANDOM
EXAMPLE_PLUGINS += $(top_builddir)/modules/examples/sources/libthreaded-random-generator.la
endif

if ENABLE_CPP
EXAMPLE_PLUGINS += $(top_builddir)/modules/examples/sources/librandom_choice_generator.la
endif

module_LTLIBRARIES += modules/examples/libexamples.la

modules_examples_libexamples_la_SOURCES = \
  modules/examples/example-plugins.c

modules_examples_libexamples_la_LIBADD = $(MODULE_DEPS_LIBS) $(EXAMPLE_PLUGINS)
EXTRA_modules_examples_libexamples_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(EXAMPLE_PLUGINS)
modules_examples_libexamples_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_CPP
nodist_EXTRA_modules_examples_libexamples_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp
endif

EXTRA_DIST += modules/examples/CMakeLists.txt

modules/examples modules/examples/ mod-examples: modules/examples/libexamples.la
else
modules/examples modules/examples/ mod-examples:
endif

.PHONY: modules/examples/ mod-examples
