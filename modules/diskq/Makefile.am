bin_PROGRAMS += modules/diskq/dqtool
noinst_LTLIBRARIES += modules/diskq/libsyslog-ng-disk-buffer.la
module_LTLIBRARIES += modules/diskq/libdisk-buffer.la

modules_diskq_libsyslog_ng_disk_buffer_la_SOURCES = \
  modules/diskq/diskq-options.h \
  modules/diskq/diskq-options.c \
  modules/diskq/logqueue-disk.c \
  modules/diskq/logqueue-disk.h \
  modules/diskq/logqueue-disk-non-reliable.c \
  modules/diskq/logqueue-disk-non-reliable.h \
  modules/diskq/logqueue-disk-reliable.c \
  modules/diskq/logqueue-disk-reliable.h \
  modules/diskq/qdisk.h \
  modules/diskq/qdisk.c

modules_diskq_libsyslog_ng_disk_buffer_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/diskq
modules_diskq_libsyslog_ng_disk_buffer_la_LIBADD	=	\
  $(MODULE_DEPS_LIBS)
modules_diskq_libsyslog_ng_disk_buffer_la_DEPENDENCIES	=	\
  $(MODULE_DEPS_LIBS)

modules_diskq_libdisk_buffer_la_SOURCES = \
  modules/diskq/diskq.c \
  modules/diskq/diskq.h \
  modules/diskq/diskq-grammar.y \
  modules/diskq/diskq-parser.c \
  modules/diskq/diskq-parser.h \
  modules/diskq/diskq-plugin.c

BUILT_SOURCES += \
  modules/diskq/diskq-grammar.y \
  modules/diskq/diskq-grammar.c \
  modules/diskq/diskq-grammar.h

EXTRA_DIST += modules/diskq/diskq-grammar.ym	\
	modules/diskq/CMakeLists.txt

LIBSYSLOG_NG_DISK_BUFFER = $(top_builddir)/modules/diskq/libsyslog-ng-disk-buffer.la

modules_diskq_libdisk_buffer_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/diskq -I$(top_builddir)/modules/diskq
modules_diskq_libdisk_buffer_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBSYSLOG_NG_DISK_BUFFER)
modules_diskq_libdisk_buffer_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_diskq_libdisk_buffer_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(LIBSYSLOG_NG_DISK_BUFFER)

modules_diskq_dqtool_SOURCES = modules/diskq/dqtool.c
modules_diskq_dqtool_LDADD = \
  $(MODULE_DEPS_LIBS) \
  $(TOOL_DEPS_LIBS) \
  $(LIBSYSLOG_NG_DISK_BUFFER)

modules/diskq modules/diskq/ mod-diskq: modules/diskq/libdisk-buffer.la \
  modules/diskq/dqtool

include modules/diskq/tests/Makefile.am

.PHONY: modules/diskq/ mod-diskq
