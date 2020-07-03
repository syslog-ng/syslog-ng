# noinst: Built as part of a larger libexamples module as static library
noinst_LTLIBRARIES      += modules/examples/destinations/example_destination/libexample_destination.la
modules_examples_destinations_example_destination_libexample_destination_la_SOURCES = \
  modules/examples/destinations/example_destination/example_destination-grammar.y       \
  modules/examples/destinations/example_destination/example_destination-parser.c        \
  modules/examples/destinations/example_destination/example_destination-parser.h        \
  modules/examples/destinations/example_destination/example_destination-plugin.c        \
  modules/examples/destinations/example_destination/example_destination.h               \
  modules/examples/destinations/example_destination/example_destination.c

BUILT_SOURCES       +=      \
  modules/examples/destinations/example_destination/example_destination-grammar.y       \
  modules/examples/destinations/example_destination/example_destination-grammar.c       \
  modules/examples/destinations/example_destination/example_destination-grammar.h

EXTRA_DIST        +=      \
  modules/examples/destinations/example_destination/example_destination-grammar.ym \
  modules/examples/destinations/example_destination/CMakeLists.txt

modules_examples_destinations_example_destination_libexample_destination_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/examples/destinations/example_destination        \
  -I$(top_builddir)/modules/examples/destinations/example_destination

modules_examples_destinations_example_destination_libexample_destination_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_examples_destinations_example_destination_libexample_destination_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_examples_destinations_example_destination_libexample_destination_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)
