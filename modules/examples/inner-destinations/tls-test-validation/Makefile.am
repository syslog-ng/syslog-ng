noinst_LTLIBRARIES += modules/examples/inner-destinations/tls-test-validation/libtls-test-validation.la

modules_examples_inner_destinations_tls_test_validation_libtls_test_validation_la_SOURCES = \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation.c \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation.h \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar.y \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-parser.c \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-parser.h

BUILT_SOURCES += \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar.y \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar.c \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar.h

EXTRA_DIST += \
  modules/examples/inner-destinations/tls-test-validation/CMakeLists.txt \
  modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar.ym

modules_examples_inner_destinations_tls_test_validation_libtls_test_validation_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/examples/inner-destinations/tls-test-validation -I$(top_builddir)/modules/examples/inner-destinations/tls-test-validation
modules_examples_inner_destinations_tls_test_validation_libtls_test_validation_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_examples_inner_destinations_tls_test_validation_libtls_test_validation_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_examples_inner_destinations_tls_test_validation_libtls_test_validation_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

PHONY: modules/examples/inner-destinations/tls-test-validation/ mod-tls-test-validation
