noinst_LTLIBRARIES += modules/examples/inner-destinations/http-test-slots/libhttp-test-slots.la

modules_examples_inner_destinations_http_test_slots_libhttp_test_slots_la_SOURCES = \
  modules/examples/inner-destinations/http-test-slots/http-test-slots.c \
  modules/examples/inner-destinations/http-test-slots/http-test-slots.h \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar.y \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-parser.c \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-parser.h

BUILT_SOURCES += \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar.y \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar.c \
  modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar.h

EXTRA_DIST += modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar.ym

modules_examples_inner_destinations_http_test_slots_libhttp_test_slots_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/examples/inner-destinations/http-test-slots -I$(top_builddir)/modules/examples/inner-destinations/http-test-slots
modules_examples_inner_destinations_http_test_slots_libhttp_test_slots_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_examples_inner_destinations_http_test_slots_libhttp_test_slots_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_examples_inner_destinations_http_test_slots_libhttp_test_slots_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

PHONY: modules/examples/inner-destinations/http-test-slots/ mod-http-test-slots
