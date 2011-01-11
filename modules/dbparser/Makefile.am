SUBDIRS = . tests
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
#
# The radix tree uses unsigned char internally but passes these values to
# string.h functions, which trigger a lot of harmless warnings.  Of course
# these could be fixed by adding a lot of casts to the code, but I
# considered these to be less readable, than simply adding -Wno-pointer-sign
# warning option.
#
AM_CFLAGS = -Wno-pointer-sign
export top_srcdir

module_LTLIBRARIES := libdbparser.la
libdbparser_la_SOURCES = \
	dbparser.c dbparser.h \
	radix.c radix.h \
	patterndb.c patterndb.h \
	dbparser-grammar.y dbparser-parser.c dbparser-parser.h dbparser-plugin.c \
	timerwheel.c timerwheel.h

libdbparser_la_CPPFLAGS = $(AM_CPPFLAGS)
libdbparser_la_LIBADD = ../../lib/libsyslog-ng.la
libdbparser_la_LDFLAGS = -avoid-version -module -no-undefined

bin_PROGRAMS = pdbtool
pdbtool_SOURCES = pdbtool.c patternize.c patternize.h
pdbtool_CPPFLAGS = $(AM_CPPFLAGS) @OPENSSL_CFLAGS@
pdbtool_LDADD = ../../lib/libsyslog-ng.la -dlpreopen libdbparser.la @OPENSSL_LIBS@ @DEPS_LIBS@

BUILT_SOURCES = dbparser-grammar.y dbparser-grammar.c dbparser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) radix-find.c dbparser-grammar.ym


include $(top_srcdir)/build/lex-rules.am
