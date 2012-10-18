moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libaffile.la
libaffile_la_SOURCES =					\
	logproto-linux-proc-kmsg-reader.h		\
	logproto-file-writer.c logproto-file-writer.h	\
	affile-common.c affile-common.h			\
	affile-source.c affile-source.h			\
	affile-dest.c affile-dest.h			\
	affile-grammar.y				\
	affile-parser.c affile-parser.h			\
	affile-plugin.c

BUILT_SOURCES = affile-grammar.y affile-grammar.c affile-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) affile-grammar.ym

libaffile_la_CPPFLAGS = $(AM_CPPFLAGS)
libaffile_la_LIBADD = $(MODULE_DEPS_LIBS)
libaffile_la_LDFLAGS = $(MODULE_LDFLAGS)

include $(top_srcdir)/build/lex-rules.am
