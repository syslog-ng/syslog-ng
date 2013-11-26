module_LTLIBRARIES			+= modules/affile/libaffile.la
modules_affile_libaffile_la_SOURCES	=			\
	modules/affile/logproto-linux-proc-kmsg-reader.h	\
	modules/affile/logproto-file-writer.c 			\
	modules/affile/logproto-file-writer.h			\
	modules/affile/poll-file-changes.c			\
	modules/affile/poll-file-changes.h			\
	modules/affile/affile-common.c				\
	modules/affile/affile-common.h				\
	modules/affile/affile-source.c				\
	modules/affile/affile-source.h				\
	modules/affile/affile-dest.c				\
	modules/affile/affile-dest.h				\
	modules/affile/affile-grammar.y				\
	modules/affile/affile-parser.c				\
	modules/affile/affile-parser.h				\
	modules/affile/affile-plugin.c

BUILT_SOURCES				+= 			\
	modules/affile/affile-grammar.y				\
	modules/affile/affile-grammar.c				\
	modules/affile/affile-grammar.h
EXTRA_DIST				+=			\
	modules/affile/affile-grammar.ym

modules_affile_libaffile_la_CPPFLAGS	=			\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)/modules/affile				\
	-I$(top_builddir)/modules/affile
modules_affile_libaffile_la_LIBADD	= $(MODULE_DEPS_LIBS)
modules_affile_libaffile_la_LDFLAGS	= $(MODULE_LDFLAGS)
modules_affile_libaffile_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/affile modules/affile/ mod-affile mod-file: modules/affile/libaffile.la
.PHONY: modules/affile/ mod-affile mod-file

include modules/affile/tests/Makefile.am
