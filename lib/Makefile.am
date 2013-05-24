DIST_SUBDIRS				= lib/ivykis

include lib/logproto/Makefile.am
include lib/rewrite/Makefile.am

lib_LTLIBRARIES				+= lib/libsyslog-ng.la
lib_libsyslog_ng_la_LIBADD		= @CORE_DEPS_LIBS@ $(libsystemd_daemon_LIBS)
lib_libsyslog_ng_la_LDFLAGS		= -no-undefined -release @VERSION@

if IVYKIS_INTERNAL
lib_libsyslog_ng_la_DEPENDENCIES	= lib/ivykis/src/libivykis.la

lib/ivykis/src/libivykis.la:
	${MAKE} -C lib/ivykis

CLEAN_SUBDIRS				+= @IVYKIS_SUBDIRS@
endif

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
pkginclude_HEADERS			+= \
	lib/afinter.h			\
	lib/alarms.h			\
	lib/apphook.h			\
	lib/atomic.h			\
	lib/block-ref-parser.h		\
	lib/cfg.h			\
	lib/cfg-grammar.h		\
	lib/cfg-lexer.h			\
	lib/cfg-lexer-subst.h		\
	lib/cfg-args.h			\
	lib/cfg-parser.h		\
	lib/cfg-tree.h			\
	lib/children.h			\
	lib/compat.h			\
	lib/control.h			\
	lib/crypto.h			\
	lib/dnscache.h			\
	lib/driver.h			\
	lib/file-perms.h		\
	lib/filter-expr-parser.h	\
	lib/filter.h			\
	lib/gprocess.h			\
	lib/gsockaddr.h			\
	lib/gsocket.h			\
	lib/logmatcher.h		\
	lib/logmpx.h			\
	lib/logmsg.h			\
	lib/logparser.h			\
	lib/logpipe.h			\
	lib/logqueue-fifo.h		\
	lib/logqueue.h			\
	lib/logreader.h			\
	lib/logsource.h			\
	lib/logstamp.h			\
	lib/logtransport.h		\
	lib/logwriter.h			\
	lib/mainloop.h			\
	lib/memtrace.h			\
	lib/messages.h			\
	lib/misc.h			\
	lib/ml-batched-timer.h		\
	lib/msg-format.h		\
	lib/nvtable.h			\
	lib/parser-expr-parser.h	\
	lib/persist-state.h		\
	lib/plugin.h			\
	lib/plugin-types.h		\
	lib/pragma-parser.h		\
	lib/scratch-buffers.h		\
	lib/serialize.h			\
	lib/stats.h			\
	lib/str-format.h		\
	lib/syslog-names.h		\
	lib/syslog-ng.h			\
	lib/tags.h			\
	lib/templates.h			\
	lib/timeutils.h			\
	lib/tls-support.h		\
	lib/tlscontext.h  		\
	lib/tlstransport.h		\
	lib/utils.h			\
	lib/uuid.h			\
	lib/value-pairs.h		\
	lib/vptransform.h		\
	lib/versioning.h		\
	$(logproto_headers)
	$(rewrite_headers)

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
lib_libsyslog_ng_crypto_la_sources	= \
	lib/crypto.c			\
	lib/tlscontext.c		\
	lib/tlstransport.c		\
	lib/uuid.c

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
lib_libsyslog_ng_la_SOURCES		= \
	lib/afinter.c			\
	lib/alarms.c			\
	lib/apphook.c			\
	lib/block-ref-parser.c		\
	lib/cfg.c			\
	lib/cfg-args.c		\
	lib/cfg-lexer.c			\
	lib/cfg-lexer-subst.c		\
	lib/cfg-parser.c		\
	lib/cfg-tree.c			\
	lib/children.c			\
	lib/compat.c			\
	lib/control.c			\
	lib/dnscache.c			\
	lib/driver.c			\
	lib/file-perms.c		\
	lib/filter.c			\
	lib/filter-expr-parser.c	\
	lib/globals.c			\
	lib/gprocess.c			\
	lib/gsockaddr.c			\
	lib/gsocket.c			\
	lib/logmatcher.c		\
	lib/logmpx.c			\
	lib/logmsg.c			\
	lib/logparser.c			\
	lib/logpipe.c			\
	lib/logqueue.c			\
	lib/logqueue-fifo.c		\
	lib/logreader.c			\
	lib/logsource.c			\
	lib/logstamp.c			\
	lib/logtransport.c		\
	lib/logwriter.c			\
	lib/mainloop.c			\
	lib/memtrace.c			\
	lib/messages.c			\
	lib/misc.c			\
	lib/ml-batched-timer.c		\
	lib/msg-format.c		\
	lib/nvtable.c			\
	lib/parser-expr-parser.c	\
	lib/persist-state.c		\
	lib/plugin.c			\
	lib/pragma-parser.c		\
	lib/scratch-buffers.c		\
	lib/serialize.c			\
	lib/stats.c			\
	lib/str-format.c		\
	lib/syslog-names.c		\
	lib/tags.c			\
	lib/templates.c			\
	lib/timeutils.c			\
	lib/utils.c			\
	lib/value-pairs.c		\
	lib/vptransform.c		\
					\
	lib/cfg-lex.l			\
	lib/cfg-grammar.y		\
	lib/filter-expr-grammar.y	\
	lib/block-ref-grammar.y		\
	lib/pragma-grammar.y		\
	lib/parser-expr-grammar.y	\
	$(logproto_sources)		\
	$(rewrite_sources)

if WITH_EMBEDDED_CRYPTO
lib_libsyslog_ng_la_CFLAGS		= @UUID_CFLAGS@ $(libsystemd_daemon_CFLAGS)
lib_libsyslog_ng_la_LIBADD		+= @OPENSSL_LIBS@ @UUID_LIBS@
lib_libsyslog_ng_la_SOURCES		+= ${lib_libsyslog_ng_crypto_la_sources}
else
module_LTLIBRARIES			+= lib/libsyslog-ng-crypto.la
lib_libsyslog_ng_crypto_la_CFLAGS	= @UUID_CFLAGS@ $(libsystemd_daemon_CFLAGS)
lib_libsyslog_ng_crypto_la_LIBADD	= @MODULE_DEPS_LIBS@ @OPENSSL_LIBS@ @UUID_LIBS@
lib_libsyslog_ng_crypto_la_LDFLAGS	= -no-undefined -avoid-version
lib_libsyslog_ng_crypto_la_DEPENDENCIES	= lib/libsyslog-ng.la
lib_libsyslog_ng_crypto_la_SOURCES	= ${lib_libsyslog_ng_crypto_la_sources}
endif

# each line with closely related files (e.g. the ones generated from the same source)
BUILT_SOURCES += lib/cfg-lex.c lib/cfg-lex.h						\
	lib/cfg-grammar.c lib/cfg-grammar.h						\
	lib/filter-expr-grammar.y lib/filter-expr-grammar.c lib/filter-expr-grammar.h	\
	lib/block-ref-grammar.y lib/block-ref-grammar.c lib/block-ref-grammar.h		\
	lib/pragma-grammar.y lib/pragma-grammar.h lib/pragma-grammar.c			\
	lib/parser-expr-grammar.y lib/parser-expr-grammar.c lib/parser-expr-grammar.h

EXTRA_DIST += \
	lib/filter-expr-grammar.ym	\
	lib/block-ref-grammar.ym	\
	lib/pragma-grammar.ym		\
	lib/parser-expr-grammar.ym	\
	lib/merge-grammar.pl		\
	lib/ivykis/configure.gnu

lib/plugin-types.h: lib/cfg-grammar.h

lib/ libsyslog-ng: lib/libsyslog-ng.la lib/libsyslog-ng-crypto.la
if IVYKIS_INTERNAL
lib/ivykis/ ivykis: lib/ivykis/src/libivykis.la
endif

.PHONY: lib/ libsyslog-ng ivykis lib/ivykis/

include lib/tests/Makefile.am
