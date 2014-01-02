DIST_SUBDIRS				= lib/ivykis

include lib/transport/Makefile.am
include lib/logproto/Makefile.am
include lib/rewrite/Makefile.am
include lib/parser/Makefile.am
include lib/filter/Makefile.am
include lib/template/Makefile.am
include lib/stats/Makefile.am
include lib/control/Makefile.am
include lib/compat/Makefile.am

lib_LTLIBRARIES				+= lib/libsyslog-ng.la
lib_libsyslog_ng_la_LIBADD		= @CORE_DEPS_LIBS@ $(libsystemd_daemon_LIBS)
lib_libsyslog_ng_la_LDFLAGS		= -no-undefined -release @VERSION@

lib_test_subdirs			= lib_filter lib_logproto lib_parser lib_rewrite lib_template lib_stats lib_control

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
	lib/cache.h			\
	lib/cfg.h			\
	lib/cfg-grammar.h		\
	lib/cfg-lexer.h			\
	lib/cfg-lexer-subst.h		\
	lib/cfg-args.h			\
	lib/cfg-parser.h		\
	lib/cfg-tree.h			\
	lib/children.h			\
	lib/crypto.h			\
	lib/dnscache.h			\
	lib/driver.h			\
	lib/file-perms.h		\
	lib/gprocess.h			\
	lib/gsockaddr.h			\
	lib/gsocket.h			\
	lib/hostname.h			\
	lib/host-resolve.h		\
	lib/logmatcher.h		\
	lib/logmpx.h			\
	lib/logmsg.h			\
	lib/logpipe.h			\
	lib/logqueue-fifo.h		\
	lib/logqueue.h			\
	lib/logreader.h			\
	lib/logsource.h			\
	lib/logstamp.h			\
	lib/logthrdestdrv.h		\
	lib/logwriter.h			\
	lib/mainloop.h			\
	lib/mainloop-call.h		\
	lib/mainloop-worker.h		\
	lib/mainloop-io-worker.h	\
	lib/memtrace.h			\
	lib/messages.h			\
	lib/misc.h			\
	lib/ml-batched-timer.h		\
	lib/msg-format.h		\
	lib/nvtable.h			\
	lib/parse-number.h		\
	lib/persist-state.h		\
	lib/persistable-state-header.h  \
	lib/plugin.h			\
	lib/plugin-types.h		\
	lib/poll-events.h		\
	lib/poll-fd-events.h		\
	lib/pragma-parser.h		\
	lib/reloc.h			\
	lib/rcptid.h			\
	lib/run-id.h			\
	lib/scratch-buffers.h		\
	lib/serialize.h			\
	lib/service-management.h	\
	lib/str-format.h		\
	lib/syslog-names.h		\
	lib/syslog-ng.h			\
	lib/tags.h			\
	lib/timeutils.h			\
	lib/tls-support.h		\
	lib/thread-utils.h		\
	lib/tlscontext.h  		\
	lib/type-hinting.h		\
	lib/uuid.h			\
	lib/value-pairs.h		\
	lib/vptransform.h		\
	lib/versioning.h

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
lib_libsyslog_ng_crypto_la_sources	= \
	lib/crypto.c			\
	lib/tlscontext.c		\
	lib/uuid.c			\
	$(transport_crypto_sources)

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
lib_libsyslog_ng_la_SOURCES		= \
	lib/afinter.c			\
	lib/alarms.c			\
	lib/apphook.c			\
	lib/block-ref-parser.c		\
	lib/cache.c			\
	lib/cfg.c			\
	lib/cfg-args.c			\
	lib/cfg-lexer.c			\
	lib/cfg-lexer-subst.c		\
	lib/cfg-parser.c		\
	lib/cfg-tree.c			\
	lib/children.c			\
	lib/dnscache.c			\
	lib/driver.c			\
	lib/file-perms.c		\
	lib/globals.c			\
	lib/gprocess.c			\
	lib/gsockaddr.c			\
	lib/gsocket.c			\
	lib/hostname.c			\
	lib/host-resolve.c		\
	lib/logmatcher.c		\
	lib/logmpx.c			\
	lib/logmsg.c			\
	lib/logpipe.c			\
	lib/logqueue.c			\
	lib/logqueue-fifo.c		\
	lib/logreader.c			\
	lib/logsource.c			\
	lib/logstamp.c			\
	lib/logthrdestdrv.c		\
	lib/logwriter.c			\
	lib/mainloop.c			\
	lib/mainloop-call.c		\
	lib/mainloop-worker.c		\
	lib/mainloop-io-worker.c	\
	lib/memtrace.c			\
	lib/messages.c			\
	lib/misc.c			\
	lib/ml-batched-timer.c		\
	lib/msg-format.c		\
	lib/nvtable.c			\
	lib/parse-number.c		\
	lib/persist-state.c		\
	lib/plugin.c			\
	lib/poll-events.c		\
	lib/poll-fd-events.c		\
	lib/pragma-parser.c		\
	lib/rcptid.c			\
	lib/reloc.c			\
	lib/run-id.c			\
	lib/scratch-buffers.c		\
	lib/serialize.c			\
	lib/service-management.c	\
	lib/str-format.c		\
	lib/syslog-names.c		\
	lib/tags.c			\
	lib/timeutils.c			\
	lib/type-hinting.c		\
	lib/value-pairs.c		\
	lib/vptransform.c		\
					\
	lib/cfg-lex.l			\
	lib/cfg-grammar.y		\
	lib/block-ref-grammar.y		\
	lib/pragma-grammar.y		\
	$(transport_sources)		\
	$(logproto_sources)		\
	$(filter_sources)		\
	$(parser_sources)		\
	$(rewrite_sources)		\
	$(template_sources)		\
	$(stats_sources)		\
	$(control_sources)		\
	$(compat_sources)

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
	lib/block-ref-grammar.y lib/block-ref-grammar.c lib/block-ref-grammar.h		\
	lib/pragma-grammar.y lib/pragma-grammar.h lib/pragma-grammar.c

EXTRA_DIST += \
	lib/block-ref-grammar.ym	\
	lib/pragma-grammar.ym		\
	lib/merge-grammar.pl		\
	lib/hostname-unix.c		\
	lib/ivykis/configure.gnu

lib/plugin-types.h: lib/cfg-grammar.h

lib/ libsyslog-ng: lib/libsyslog-ng.la lib/libsyslog-ng-crypto.la
if IVYKIS_INTERNAL
lib/ivykis/ ivykis: lib/ivykis/src/libivykis.la
endif

.PHONY: lib/ libsyslog-ng ivykis lib/ivykis/

include lib/tests/Makefile.am
