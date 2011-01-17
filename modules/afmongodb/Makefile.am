if ENABLE_INCLUDED_MONGO_CLIENT
mongo_client_subdir = libmongo-client
mongo_client_cflags = -I${srcdir}/libmongo-client/src
mongo_client_libadd = libmongo-client/src/libmongo-client.la
endif

SUBDIRS = ${mongo_client_subdir}

moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libafmongodb.la

export top_srcdir

libafmongodb_la_CFLAGS = ${mongo_client_cflags}
libafmongodb_la_SOURCES = afmongodb-grammar.y afmongodb.c afmongodb.h afmongodb-parser.c afmongodb-parser.h
libafmongodb_la_LIBADD = ../../lib/libsyslog-ng.la ${mongo_client_libadd}
libafmongodb_la_LDFLAGS = -avoid-version

BUILT_SOURCES = afmongodb-grammar.y afmongodb-grammar.c afmongodb-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afmongodb-grammar.ym

include $(top_srcdir)/build/lex-rules.am
