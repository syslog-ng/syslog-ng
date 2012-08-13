moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libbasic-proto.la
libbasic_proto_la_SOURCES = \
	basic-proto.c basic-proto.h \
	basic-proto-grammar.y basic-proto-parser.c basic-proto-parser.h basic-proto-plugin.c

BUILT_SOURCES = basic-proto-grammar.y basic-proto-grammar.c basic-proto-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) basic-proto-grammar.ym

libbasic_proto_la_CPPFLAGS = $(AM_CPPFLAGS)
libbasic_proto_la_LIBADD = $(MODULE_DEPS_LIBS)
libbasic_proto_la_LDFLAGS = $(MODULE_LDFLAGS) -lpcre

include $(top_srcdir)/build/lex-rules.am
