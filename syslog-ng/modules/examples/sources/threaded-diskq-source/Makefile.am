noinst_LTLIBRARIES += modules/examples/sources/libthreaded-diskq-source.la

modules_examples_sources_libthreaded_diskq_source_la_SOURCES = \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar.y \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-parser.c \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-parser.h \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source.c \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source.h

BUILT_SOURCES += \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar.y \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar.c \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar.h

EXTRA_DIST += \
  modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar.ym \
  modules/examples/sources/threaded-diskq-source/CMakeLists.txt

modules_examples_sources_libthreaded_diskq_source_la_CPPFLAGS  = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/examples/sources/threaded-diskq-source \
  -I$(top_builddir)/modules/examples/sources/threaded-diskq-source \
  -I$(top_srcdir)/modules/diskq

modules_examples_sources_libthreaded_diskq_source_la_LIBADD  = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/diskq/libsyslog-ng-disk-buffer.la

modules_examples_sources_libthreaded_diskq_source_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_examples_sources_libthreaded_diskq_source_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/diskq/libsyslog-ng-disk-buffer.la
