noinst_LTLIBRARIES					+= modules/correlation/libsyslog-ng-patterndb.la

modules_correlation_libsyslog_ng_patterndb_la_SOURCES	=	\
	modules/correlation/radix.c				\
	modules/correlation/radix.h				\
	modules/correlation/patterndb.c				\
	modules/correlation/patterndb.h				\
	modules/correlation/pdb-error.c				\
	modules/correlation/pdb-error.h				\
	modules/correlation/pdb-file.c				\
	modules/correlation/pdb-file.h				\
	modules/correlation/pdb-load.c				\
	modules/correlation/pdb-load.h				\
	modules/correlation/pdb-rule.c				\
	modules/correlation/pdb-rule.h				\
	modules/correlation/pdb-action.c			\
	modules/correlation/pdb-action.h			\
	modules/correlation/pdb-program.c			\
	modules/correlation/pdb-program.h			\
	modules/correlation/pdb-example.c			\
	modules/correlation/pdb-example.h			\
	modules/correlation/pdb-ruleset.c			\
	modules/correlation/pdb-ruleset.h			\
	modules/correlation/pdb-context.c			\
	modules/correlation/pdb-context.h			\
	modules/correlation/pdb-ratelimit.c			\
	modules/correlation/pdb-ratelimit.h			\
	modules/correlation/pdb-lookup-params.h			\
	modules/correlation/pdb-lookup-params.c			\
	modules/correlation/correlation.c			\
	modules/correlation/correlation.h			\
	modules/correlation/correlation-key.c			\
	modules/correlation/correlation-key.h			\
	modules/correlation/correlation-context.c		\
	modules/correlation/correlation-context.h		\
	modules/correlation/synthetic-message.c			\
	modules/correlation/synthetic-message.h			\
	modules/correlation/synthetic-context.c			\
	modules/correlation/synthetic-context.h			\
	modules/correlation/timerwheel.c			\
	modules/correlation/timerwheel.h			\
	modules/correlation/patternize.c			\
	modules/correlation/patternize.h      \
	modules/correlation/id-counter.h      \
	modules/correlation/id-counter.c

modules_correlation_libsyslog_ng_patterndb_la_CFLAGS	=	\
	$(AM_CFLAGS) -fPIC
modules_correlation_libsyslog_ng_patterndb_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
EXTRA_modules_correlation_libsyslog_ng_patterndb_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

module_LTLIBRARIES					+= modules/correlation/libcorrelation.la
modules_correlation_libcorrelation_la_SOURCES			=	\
	modules/correlation/stateful-parser.c			\
	modules/correlation/stateful-parser.h			\
	modules/correlation/dbparser.c				\
	modules/correlation/dbparser.h				\
	modules/correlation/correlation-grammar.y		\
	modules/correlation/correlation-parser.c		\
	modules/correlation/correlation-parser.h		\
	modules/correlation/correlation-plugin.c		\
	modules/correlation/grouping-parser.c			\
	modules/correlation/grouping-parser.h			\
	modules/correlation/groupingby.c			\
	modules/correlation/groupingby.h			\
	modules/correlation/group-lines.c			\
	modules/correlation/group-lines.h			\
	$(modules_correlation_libsyslog_ng_patterndb_la_SOURCES)
modules_correlation_libcorrelation_la_CPPFLAGS		=	\
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)/modules/correlation			\
	-I$(top_builddir)/modules/correlation
modules_correlation_libcorrelation_la_LIBADD			=	\
	$(MODULE_DEPS_LIBS)
modules_correlation_libcorrelation_la_LDFLAGS			=	\
	$(MODULE_LDFLAGS)
EXTRA_modules_correlation_libcorrelation_la_DEPENDENCIES		=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES						+=	\
	modules/correlation/correlation-grammar.y			\
	modules/correlation/correlation-grammar.c			\
	modules/correlation/correlation-grammar.h
EXTRA_DIST						+=	\
	modules/correlation/correlation-grammar.ym	\
	modules/correlation/CMakeLists.txt

modules/correlation modules/correlation/ mod-correlation: modules/correlation/libcorrelation.la \
						 modules/correlation/pdbtool/pdbtool
.PHONY: modules/correlation/ mod-correlation

include modules/correlation/pdbtool/Makefile.am
include modules/correlation/tests/Makefile.am
