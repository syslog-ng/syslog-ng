noinst_LTLIBRARIES					+= modules/dbparser/libsyslog-ng-patterndb.la

modules_dbparser_libsyslog_ng_patterndb_la_SOURCES	=	\
	modules/dbparser/radix.c				\
	modules/dbparser/radix.h				\
	modules/dbparser/patterndb.c				\
	modules/dbparser/patterndb.h				\
	modules/dbparser/pdb-error.c				\
	modules/dbparser/pdb-error.h				\
	modules/dbparser/pdb-file.c				\
	modules/dbparser/pdb-file.h				\
	modules/dbparser/pdb-load.c				\
	modules/dbparser/pdb-load.h				\
	modules/dbparser/pdb-rule.c				\
	modules/dbparser/pdb-rule.h				\
	modules/dbparser/pdb-action.c				\
	modules/dbparser/pdb-action.h				\
	modules/dbparser/pdb-program.c				\
	modules/dbparser/pdb-program.h				\
	modules/dbparser/pdb-example.c				\
	modules/dbparser/pdb-example.h				\
	modules/dbparser/pdb-ruleset.c				\
	modules/dbparser/pdb-ruleset.h				\
	modules/dbparser/pdb-context.c				\
	modules/dbparser/pdb-context.h				\
	modules/dbparser/pdb-ratelimit.c			\
	modules/dbparser/pdb-ratelimit.h			\
	modules/dbparser/pdb-lookup-params.h			\
	modules/dbparser/correllation.c				\
	modules/dbparser/correllation.h				\
	modules/dbparser/correllation-key.c			\
	modules/dbparser/correllation-key.h			\
	modules/dbparser/correllation-context.c			\
	modules/dbparser/correllation-context.h			\
	modules/dbparser/synthetic-message.c			\
	modules/dbparser/synthetic-message.h			\
	modules/dbparser/synthetic-context.c			\
	modules/dbparser/synthetic-context.h			\
	modules/dbparser/timerwheel.c				\
	modules/dbparser/timerwheel.h				\
	modules/dbparser/patternize.c				\
	modules/dbparser/patternize.h

modules_dbparser_libsyslog_ng_patterndb_la_CFLAGS	=	\
	$(AM_CFLAGS) -fPIC
modules_dbparser_libsyslog_ng_patterndb_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_dbparser_libsyslog_ng_patterndb_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

module_LTLIBRARIES					+= modules/dbparser/libdbparser.la
modules_dbparser_libdbparser_la_SOURCES			=	\
	modules/dbparser/stateful-parser.c			\
	modules/dbparser/stateful-parser.h			\
	modules/dbparser/dbparser.c				\
	modules/dbparser/dbparser.h				\
	modules/dbparser/dbparser-grammar.y			\
	modules/dbparser/dbparser-parser.c			\
	modules/dbparser/dbparser-parser.h			\
	modules/dbparser/dbparser-plugin.c			\
	modules/dbparser/groupingby.c				\
	modules/dbparser/groupingby.h				\
	$(modules_dbparser_libsyslog_ng_patterndb_la_SOURCES)
modules_dbparser_libdbparser_la_CPPFLAGS		=	\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)/modules/dbparser			\
	-I$(top_builddir)/modules/dbparser
modules_dbparser_libdbparser_la_LIBADD			=	\
	$(MODULE_DEPS_LIBS)
modules_dbparser_libdbparser_la_LDFLAGS			=	\
	$(MODULE_LDFLAGS)
modules_dbparser_libdbparser_la_DEPENDENCIES		=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES						+=	\
	modules/dbparser/dbparser-grammar.y			\
	modules/dbparser/dbparser-grammar.c			\
	modules/dbparser/dbparser-grammar.h
EXTRA_DIST						+=	\
	modules/dbparser/dbparser-grammar.ym	\
	modules/dbparser/CMakeLists.txt

modules/dbparser modules/dbparser/ mod-dbparser: modules/dbparser/libdbparser.la \
						 modules/dbparser/pdbtool/pdbtool
.PHONY: modules/dbparser/ mod-dbparser

include modules/dbparser/pdbtool/Makefile.am
include modules/dbparser/tests/Makefile.am
