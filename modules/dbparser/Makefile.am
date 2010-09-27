SUBDIRS = . tests
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libdbparser.la
libdbparser_la_SOURCES = \
	dbparser.c dbparser.h \
	radix.c radix.h \
	logpatterns.c logpatterns.h \
	dbparser-grammar.y dbparser-parser.c dbparser-parser.h dbparser-plugin.c \
	timerwheel.c timerwheel.h

libdbparser_la_CPPFLAGS = $(AM_CPPFLAGS)
libdbparser_la_LIBADD = ../../lib/libsyslog-ng.la
libdbparser_la_LDFLAGS = -avoid-version -module

bin_PROGRAMS = pdbtool
pdbtool_SOURCES = pdbtool.c
pdbtool_LDADD = ../../lib/libsyslog-ng.la libdbparser.la @DEPS_LIBS@

BUILT_SOURCES = dbparser-grammar.y dbparser-grammar.c dbparser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) radix-find.c dbparser-grammar.ym


include $(top_srcdir)/build/lex-rules.am
