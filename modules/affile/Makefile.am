module_LTLIBRARIES			+= modules/affile/libaffile.la
modules_affile_libaffile_la_SOURCES	=			\
	modules/affile/logproto-file-writer.c 			\
	modules/affile/logproto-file-writer.h			\
	modules/affile/logproto-file-reader.c 			\
	modules/affile/logproto-file-reader.h			\
	modules/affile/poll-file-changes.c			\
	modules/affile/poll-file-changes.h			\
	modules/affile/transport-prockmsg.c			\
	modules/affile/transport-prockmsg.h			\
	modules/affile/file-reader.c				\
	modules/affile/file-reader.h				\
	modules/affile/wildcard-file-reader.c		\
	modules/affile/wildcard-file-reader.h		\
	modules/affile/wildcard-source.h			\
	modules/affile/wildcard-source.c			\
	modules/affile/directory-monitor.h			\
	modules/affile/directory-monitor.c			\
	modules/affile/directory-monitor-factory.h			\
	modules/affile/directory-monitor-factory.c			\
	modules/affile/collection-comparator.h		\
	modules/affile/collection-comparator.c		\
	modules/affile/directory-monitor-poll.h   \
	modules/affile/directory-monitor-poll.c   \
	modules/affile/file-opener.c				\
	modules/affile/file-opener.h				\
	modules/affile/file-specializations.h			\
	modules/affile/regular-files.c				\
	modules/affile/named-pipe.c				\
	modules/affile/named-pipe.h				\
	modules/affile/linux-kmsg.c				\
	modules/affile/stdin.c					\
	modules/affile/stdin.h					\
	modules/affile/affile-source.c				\
	modules/affile/affile-source.h				\
	modules/affile/affile-dest.c				\
	modules/affile/affile-dest.h				\
	modules/affile/affile-grammar.y				\
	modules/affile/affile-parser.c				\
	modules/affile/affile-parser.h				\
	modules/affile/affile-plugin.c \
	modules/affile/file-list.h				\
	modules/affile/file-list.c				\
  modules/affile/affile-dest-internal-queue-filter.h

if HAVE_INOTIFY
  modules_affile_libaffile_la_SOURCES +=      \
  modules/affile/directory-monitor-inotify.h  \
  modules/affile/directory-monitor-inotify.c
else
  EXTRA_DIST +=                               \
  modules/affile/directory-monitor-inotify.h  \
  modules/affile/directory-monitor-inotify.c
endif

BUILT_SOURCES				+= 			\
	modules/affile/affile-grammar.y				\
	modules/affile/affile-grammar.c				\
	modules/affile/affile-grammar.h
EXTRA_DIST				+=			\
	modules/affile/affile-grammar.ym	\
	modules/affile/CMakeLists.txt

modules_affile_libaffile_la_CPPFLAGS	=			\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)/modules/affile				\
	-I$(top_builddir)/modules/affile
modules_affile_libaffile_la_LIBADD	= $(MODULE_DEPS_LIBS) $(IVYKIS_LIBS)
modules_affile_libaffile_la_LDFLAGS	= $(MODULE_LDFLAGS)
modules_affile_libaffile_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/affile modules/affile/ mod-affile mod-file: modules/affile/libaffile.la
.PHONY: modules/affile/ mod-affile mod-file

include modules/affile/tests/Makefile.am
