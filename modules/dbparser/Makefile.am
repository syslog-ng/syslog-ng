noinst_LTLIBRARIES					+= modules/dbparser/libsyslog-ng-patterndb.la

modules_dbparser_libsyslog_ng_patterndb_la_SOURCES	=	\
	modules/dbparser/radix.c				\
	modules/dbparser/radix.h				\
	modules/dbparser/patterndb.c				\
	modules/dbparser/patterndb.h				\
	modules/dbparser/patterndb-int.h			\
	modules/dbparser/timerwheel.c				\
	modules/dbparser/timerwheel.h				\
	modules/dbparser/patternize.c				\
	modules/dbparser/patternize.h

# The radix tree uses unsigned char internally but passes these values to
# string.h functions, which trigger a lot of harmless warnings.  Of course
# these could be fixed by adding a lot of casts to the code, but I
# considered these to be less readable, than simply adding -Wno-pointer-sign
# warning option.
#
modules_dbparser_libsyslog_ng_patterndb_la_CFLAGS	=	\
	$(AM_CFLAGS) -fPIC @CFLAGS_NOWARN_POINTER_SIGN@
modules_dbparser_libsyslog_ng_patterndb_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)					\
	$(top_builddir)/lib/libsyslog-ng-crypto.la
modules_dbparser_libsyslog_ng_patterndb_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS) lib/libsyslog-ng-crypto.la

module_LTLIBRARIES					+= modules/dbparser/libdbparser.la
modules_dbparser_libdbparser_la_SOURCES			=	\
	modules/dbparser/dbparser.c				\
	modules/dbparser/dbparser.h				\
	modules/dbparser/dbparser-grammar.y			\
	modules/dbparser/dbparser-parser.c			\
	modules/dbparser/dbparser-parser.h			\
	modules/dbparser/dbparser-plugin.c			\
	$(modules_dbparser_libsyslog_ng_patterndb_la_SOURCES)
modules_dbparser_libdbparser_la_CPPFLAGS		=	\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)/modules/dbparser			\
	-I$(top_builddir)/modules/dbparser			\
	@CFLAGS_NOWARN_POINTER_SIGN@
modules_dbparser_libdbparser_la_LIBADD			=	\
	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS)
modules_dbparser_libdbparser_la_LDFLAGS			=	\
	$(MODULE_LDFLAGS)
modules_dbparser_libdbparser_la_DEPENDENCIES		=	\
	$(MODULE_DEPS_LIBS) lib/libsyslog-ng-crypto.la

BUILT_SOURCES						+=	\
	modules/dbparser/dbparser-grammar.y			\
	modules/dbparser/dbparser-grammar.c			\
	modules/dbparser/dbparser-grammar.h
EXTRA_DIST						+=	\
	modules/dbparser/radix-find.c				\
	modules/dbparser/dbparser-grammar.ym

modules/dbparser modules/dbparser/ mod-dbparser: modules/dbparser/libdbparser.la \
						 modules/dbparser/pdbtool/pdbtool
.PHONY: modules/dbparser/ mod-dbparser

include modules/dbparser/pdbtool/Makefile.am
include modules/dbparser/tests/Makefile.am
