noinst_LTLIBRARIES += modules/examples/sources/libmsg-generator.la

modules_examples_sources_libmsg_generator_la_SOURCES = \
  modules/examples/sources/msg-generator/msg-generator-grammar.y \
  modules/examples/sources/msg-generator/msg-generator-parser.c \
  modules/examples/sources/msg-generator/msg-generator-parser.h \
  modules/examples/sources/msg-generator/msg-generator.c \
  modules/examples/sources/msg-generator/msg-generator.h \
  modules/examples/sources/msg-generator/msg-generator-source.c \
  modules/examples/sources/msg-generator/msg-generator-source.h \
  modules/examples/sources/msg-generator/msg-generator-source-options.h

BUILT_SOURCES += \
  modules/examples/sources/msg-generator/msg-generator-grammar.y \
  modules/examples/sources/msg-generator/msg-generator-grammar.c \
  modules/examples/sources/msg-generator/msg-generator-grammar.h

EXTRA_DIST += \
  modules/examples/sources/msg-generator/msg-generator-grammar.ym \
  modules/examples/sources/msg-generator/CMakeLists.txt

modules_examples_sources_libmsg_generator_la_CPPFLAGS  = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/examples/sources/msg-generator \
  -I$(top_builddir)/modules/examples/sources/msg-generator

modules_examples_sources_libmsg_generator_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_examples_sources_libmsg_generator_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_examples_sources_libmsg_generator_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)
