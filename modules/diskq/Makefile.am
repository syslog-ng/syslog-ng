BUILT_SOURCES += modules/diskq/diskq-grammar.y modules/diskq/diskq-grammar.c modules/diskq/diskq-grammar.h
EXTRA_DIST += $(BUILT_SOURCES) modules/diskq/diskq-grammar.ym

module_LTLIBRARIES += modules/diskq/libdisk-buffer.la
bin_PROGRAMS += modules/diskq/dqtool

modules_diskq_libdisk_buffer_la_SOURCES = \
  modules/diskq/diskq.c \
  modules/diskq/diskq.h \
  modules/diskq/diskq-grammar.y \
  modules/diskq/diskq-options.h \
  modules/diskq/diskq-options.c \
  modules/diskq/diskq-parser.c \
  modules/diskq/diskq-parser.h \
  modules/diskq/diskq-plugin.c \
  modules/diskq/logqueue-disk.c \
  modules/diskq/logqueue-disk.h \
  modules/diskq/logqueue-disk-non-reliable.c \
  modules/diskq/logqueue-disk-non-reliable.h \
  modules/diskq/logqueue-disk-reliable.c \
  modules/diskq/logqueue-disk-reliable.h \
  modules/diskq/qdisk.h \
  modules/diskq/qdisk.c

modules_diskq_libdisk_buffer_la_CFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/diskq -I$(top_builddir)/modules/diskq -I$(top_srcdir)/lib -I../../lib
modules_diskq_libdisk_buffer_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_diskq_libdisk_buffer_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_diskq_dqtool_SOURCES = modules/diskq/dqtool.c
modules_diskq_dqtool_LDADD = $(top_builddir)/modules/diskq/libdisk-buffer.la $(top_builddir)/lib/libsyslog-ng.la @BASE_LIBS@ @OPENSSL_LIBS@ @GLIB_LIBS@

modules/diskq modules/diskq/ mod-disk-buffer: modules/diskq/libdisk-buffer.la

include modules/diskq/tests/Makefile.am

.PHONY: modules/diskq/ mod-disk-buffer
