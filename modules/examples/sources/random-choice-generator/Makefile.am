if ENABLE_CPP
# C++ code

noinst_LTLIBRARIES += modules/examples/sources/librandom_choice_generator_cpp.la

modules_examples_sources_librandom_choice_generator_cpp_la_SOURCES = \
  modules/examples/sources/random-choice-generator/random-choice-generator.h \
  modules/examples/sources/random-choice-generator/random-choice-generator.hpp \
  modules/examples/sources/random-choice-generator/random-choice-generator.cpp

modules_examples_sources_librandom_choice_generator_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  -I$(top_srcdir)/modules/examples/sources/random-choice-generator \
  -I$(top_builddir)/modules/examples/sources/random-choice-generator

modules_examples_sources_librandom_choice_generator_cpp_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_examples_sources_librandom_choice_generator_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_examples_sources_librandom_choice_generator_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

# C code

noinst_LTLIBRARIES += modules/examples/sources/librandom_choice_generator.la

modules_examples_sources_librandom_choice_generator_la_SOURCES = \
  modules/examples/sources/random-choice-generator/random-choice-generator-grammar.y \
  modules/examples/sources/random-choice-generator/random-choice-generator-parser.c \
  modules/examples/sources/random-choice-generator/random-choice-generator-parser.h

modules_examples_sources_librandom_choice_generator_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/examples/sources/random-choice-generator \
  -I$(top_builddir)/modules/examples/sources/random-choice-generator

modules_examples_sources_librandom_choice_generator_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  modules/examples/sources/librandom_choice_generator_cpp.la

modules_examples_sources_librandom_choice_generator_la_LDFLAGS = \
  $(MODULE_LDFLAGS)

EXTRA_modules_examples_sources_librandom_choice_generator_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  modules/examples/sources/librandom_choice_generator_cpp.la

modules/examples/sources/random-choice-generator modules/examples/sources/random-choice-generator/ mod-random-choice-generator: modules/examples/sources/random-choice-generator/librandom_choice_generator.la
else
modules/examples/sources/random-choice-generator modules/examples/sources/random-choice-generator/ mod-random-choice-generator:
endif

BUILT_SOURCES += \
  modules/examples/sources/random-choice-generator/random-choice-generator-grammar.y \
  modules/examples/sources/random-choice-generator/random-choice-generator-grammar.c \
  modules/examples/sources/random-choice-generator/random-choice-generator-grammar.h

EXTRA_DIST += \
  modules/examples/sources/random-choice-generator/random-choice-generator-grammar.ym \
  modules/examples/sources/random-choice-generator/CMakeLists.txt
