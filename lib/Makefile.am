DIST_SUBDIRS				+= @IVYKIS_SUBDIRS@

include lib/ack-tracker/Makefile.am
include lib/eventlog/src/Makefile.am
include lib/eventlog/tests/Makefile.am
include lib/transport/Makefile.am
include lib/logproto/Makefile.am
include lib/rewrite/Makefile.am
include lib/parser/Makefile.am
include lib/filter/Makefile.am
include lib/template/Makefile.am
include lib/value-pairs/Makefile.am
include lib/stats/Makefile.am
include lib/metrics/Makefile.am
include lib/control/Makefile.am
include lib/healthcheck/Makefile.am
include lib/debugger/Makefile.am
include lib/compat/Makefile.am
include lib/logmsg/Makefile.am
include lib/scanner/csv-scanner/Makefile.am
include lib/scanner/list-scanner/Makefile.am
include lib/scanner/kv-scanner/Makefile.am
include lib/scanner/xml-scanner/Makefile.am
include lib/str-repr/Makefile.am
include lib/timeutils/Makefile.am
include lib/secret-storage/Makefile.am
include lib/logthrsource/Makefile.am
include lib/logthrdest/Makefile.am
include lib/signal-slot-connector/Makefile.am
include lib/multi-line/Makefile.am

LSNG_RELEASE		= $(shell echo @PACKAGE_VERSION@ | cut -d. -f1,2)

# * Start with version information of ‘0:0:0’ for each libtool
#   library.
# * Update the version information only immediately before a public
#   release of your software. More frequent updates are unnecessary,
#   and only guarantee that the current interface number gets larger
#   faster.
# * If the library source code has changed at all since the last
#   update, then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# * If any interfaces have been added, removed, or changed since the
#   last update, increment current, and set revision to 0.
# * If any interfaces have been added since the last public release,
#   then increment age.
# * If any interfaces have been removed or changed since the last
#   public release, then set age to 0.
LSNG_CURRENT		= 0
LSNG_REVISION		= 0
LSNG_AGE		= 0

lib_LTLIBRARIES				+= lib/libsyslog-ng.la
lib_libsyslog_ng_la_LIBADD		= @CORE_DEPS_LIBS@ $(libsystemd_LIBS) $(top_builddir)/lib/secret-storage/libsecret-storage.la
lib_libsyslog_ng_la_LDFLAGS		= -no-undefined -release ${LSNG_RELEASE} \
					  -version-info ${LSNG_CURRENT}:${LSNG_REVISION}:${LSNG_AGE}

lib_test_subdirs			= lib_filter lib_logproto lib_parser lib_rewrite lib_template lib_stats lib_control

EXTRA_lib_libsyslog_ng_la_DEPENDENCIES       = lib/eventlog/src/libevtlog.la lib/secret-storage/libsecret-storage.la

if IVYKIS_INTERNAL
EXTRA_lib_libsyslog_ng_la_DEPENDENCIES	+= lib/ivykis/src/libivykis.la

lib/ivykis/src/libivykis.la:
	${MAKE} CFLAGS="${CFLAGS} -Wno-error" -C lib/ivykis

CLEAN_SUBDIRS				+= @IVYKIS_SUBDIRS@

EXTRA_DIST += \
	lib/ivykis/configure.gnu	\
	lib/CMakeLists.txt

install-ivykis:
	${MAKE} -C lib/ivykis/src \
		install-includeHEADERS \
		install-nodist_includeHEADERS \
		includedir="${pkgincludedir}/ivykis"

uninstall-ivykis:
	${MAKE} -C lib/ivykis/src \
		uninstall-includeHEADERS \
		uninstall-nodist_includeHEADERS \
		includedir="${pkgincludedir}/ivykis"

INSTALL_EXEC_HOOKS			+= install-ivykis
UNINSTALL_HOOKS				+= uninstall-ivykis
endif


# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
pkginclude_HEADERS			+= \
	lib/afinter.h			\
	lib/alarms.h			\
	lib/apphook.h			\
	lib/atomic.h			\
	lib/atomic-gssize.h \
	lib/block-ref-parser.h		\
	lib/cache.h			\
	lib/console.h			\
	lib/cfg.h			\
	lib/cfg-grammar.h		\
	lib/cfg-grammar-internal.h	\
	lib/cfg-helpers.h		\
	lib/cfg-lexer.h			\
	lib/cfg-lexer-subst.h		\
	lib/cfg-args.h			\
	lib/cfg-block.h			\
	lib/cfg-block-generator.h	\
	lib/cfg-parser.h		\
	lib/cfg-path.h			\
	lib/cfg-source.h		\
	lib/cfg-tree.h			\
	lib/cfg-walker.h		\
	lib/cfg-persist.h		\
	lib/children.h			\
	lib/crypto.h			\
	lib/dnscache.h			\
	lib/driver.h			\
	lib/dynamic-window-pool.h \
	lib/dynamic-window.h \
	lib/fdhelpers.h			\
	lib/file-monitor.h		\
	lib/file-perms.h		\
	lib/find-crlf.h			\
	lib/generic-number.h		\
	lib/gprocess.h			\
	lib/gsockaddr.h			\
	lib/gsocket.h			\
	lib/hostname.h			\
	lib/host-resolve.h		\
	lib/list-adt.h \
	lib/logmatcher.h		\
	lib/logmpx.h			\
	lib/logscheduler.h		\
	lib/logscheduler-pipe.h		\
	lib/logpipe.h			\
	lib/logqueue-fifo.h		\
	lib/logqueue.h			\
	lib/logreader.h			\
	lib/logsource.h			\
	lib/logwriter.h			\
	lib/mainloop.h			\
	lib/mainloop-call.h		\
	lib/mainloop-worker.h		\
	lib/mainloop-threaded-worker.h	\
	lib/mainloop-io-worker.h	\
	lib/mainloop-control.h		\
	lib/module-config.h		\
	lib/memtrace.h			\
	lib/messages.h			\
	lib/metrics-pipe.h			\
	lib/ml-batched-timer.h		\
	lib/msg-format.h		\
	lib/msg-stats.h			\
	lib/notified-fd-events.h		\
	lib/on-error.h			\
	lib/parse-number.h		\
	lib/pathutils.h         \
	lib/persist-state.h		\
	lib/persistable-state-header.h  \
	lib/persistable-state-presenter.h		\
	lib/plugin.h			\
	lib/plugin-types.h		\
	lib/poll-events.h		\
	lib/poll-fd-events.h		\
	lib/pragma-parser.h		\
	lib/presented-persistable-state.h			\
	lib/reloc.h			\
	lib/rcptid.h			\
	lib/run-id.h			\
	lib/scratch-buffers.h		\
	lib/serialize.h			\
	lib/service-management.h	\
	lib/seqnum.h			\
	lib/signal-handler.h		\
	lib/stackdump.h			\
	lib/str-format.h		\
	lib/str-utils.h			\
	lib/syslog-names.h		\
	lib/syslog-ng.h			\
	lib/misc.h                      \
	lib/string-list.h		\
	lib/tls-support.h		\
	lib/thread-utils.h		\
	lib/uuid.h			\
	lib/userdb.h			\
	lib/utf8utils.h			\
	lib/versioning.h		\
	lib/ringbuffer.h		\
	lib/host-id.h			\
	lib/resolved-configurable-paths.h \
	lib/pe-versioning.h \
	lib/window-size-counter.h

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
lib_libsyslog_ng_la_SOURCES		= \
	lib/afinter.c			\
	lib/alarms.c			\
	lib/apphook.c			\
	lib/block-ref-parser.c		\
	lib/cache.c			\
	lib/console.c			\
	lib/cfg.c			\
	lib/cfg-args.c			\
	lib/cfg-block.c			\
	lib/cfg-block-generator.c	\
	lib/cfg-lexer.c			\
	lib/cfg-lexer-subst.c		\
	lib/cfg-helpers.c		\
	lib/cfg-parser.c		\
	lib/cfg-path.c			\
	lib/cfg-source.c		\
	lib/cfg-tree.c			\
	lib/cfg-walker.c		\
	lib/cfg-persist.c		\
	lib/children.c			\
	lib/dnscache.c			\
	lib/driver.c			\
	lib/dynamic-window.c \
	lib/dynamic-window-pool.c \
	lib/fdhelpers.c			\
	lib/file-monitor.c 		\
	lib/file-perms.c		\
	lib/find-crlf.c			\
	lib/generic-number.c		\
	lib/globals.c			\
	lib/gprocess.c			\
	lib/gsockaddr.c			\
	lib/gsocket.c			\
	lib/hostname.c			\
	lib/host-resolve.c		\
	lib/logmatcher.c		\
	lib/logmpx.c			\
	lib/logscheduler.c		\
	lib/logscheduler-pipe.c		\
	lib/logpipe.c			\
	lib/logqueue.c			\
	lib/logqueue-fifo.c		\
	lib/logreader.c			\
	lib/logsource.c			\
	lib/logwriter.c			\
	lib/mainloop.c			\
	lib/signal-handler.c		\
	lib/mainloop-call.c		\
	lib/mainloop-worker.c		\
	lib/mainloop-threaded-worker.c	\
	lib/mainloop-io-worker.c	\
	lib/mainloop-control.c		\
	lib/module-config.c		\
	lib/memtrace.c			\
	lib/messages.c			\
	lib/metrics-pipe.c			\
	lib/ml-batched-timer.c		\
	lib/msg-format.c		\
	lib/msg-stats.c			\
	lib/on-error.c			\
	lib/parse-number.c		\
	lib/pathutils.c         \
	lib/persist-state.c		\
	lib/plugin.c			\
	lib/poll-events.c		\
	lib/poll-fd-events.c		\
	lib/pragma-parser.c		\
	lib/persistable-state-presenter.c		\
	lib/rcptid.c			\
	lib/reloc.c			\
	lib/run-id.c			\
	lib/scratch-buffers.c		\
	lib/serialize.c			\
	lib/service-management.c	\
	lib/stackdump.c			\
	lib/str-format.c		\
	lib/str-utils.c			\
	lib/syslog-names.c		\
	lib/string-list.c		\
	lib/ringbuffer.c		\
	lib/crypto.c			\
	lib/uuid.c			\
	lib/userdb.c			\
	lib/utf8utils.c			\
	$(transport_crypto_sources)	\
	lib/host-id.c			\
	lib/resolved-configurable-paths.c \
	lib/window-size-counter.c       \
	lib/cfg-lex.l			\
	lib/cfg-grammar.y		\
	lib/cfg-grammar-internal.c	\
	lib/block-ref-grammar.y		\
	lib/pragma-grammar.y		\
	$(ack_tracker_sources) \
	$(csvscanner_sources)		\
	$(kvscanner_sources)		\
	$(listscanner_sources)		\
	$(xmlscanner_sources)		\
	$(transport_sources)		\
	$(logproto_sources)		\
	$(filter_sources)		\
	$(parser_sources)		\
	$(rewrite_sources)		\
	$(template_sources)		\
	$(value_pairs_sources)		\
	$(stats_sources)		\
	$(metrics_sources)		\
	$(control_sources)		\
	$(healthcheck_sources) \
	$(debugger_sources)		\
	$(compat_sources)     		\
	$(logmsg_sources)		\
	$(str_repr_sources)		\
	$(timeutils_sources)		\
	$(multiline_sources)		\
	$(logthrsource_sources)		\
	$(logthrdest_sources) \
	$(signal_slot_connector_sources)

lib_libsyslog_ng_la_CFLAGS		= \
	$(AM_CFLAGS) \
	$(libsystemd_CFLAGS)
lib_libsyslog_ng_la_LIBADD		+= @OPENSSL_LIBS@

# each line with closely related files (e.g. the ones generated from the same source)
BUILT_SOURCES += lib/cfg-lex.c lib/cfg-lex.h						\
	lib/cfg-grammar.c lib/cfg-grammar.h						\
	lib/block-ref-grammar.y lib/block-ref-grammar.c lib/block-ref-grammar.h		\
	lib/pragma-grammar.y lib/pragma-grammar.h lib/pragma-grammar.c			\
	lib/severity-aliases.h

EXTRA_DIST += \
	lib/block-ref-grammar.ym	\
	lib/pragma-grammar.ym		\
	lib/merge-grammar.py		\
	lib/severity-aliases.table	\
	lib/severity-aliases.h		\
	lib/hostname-unix.c

lib/plugin-types.h: lib/cfg-grammar.h

%.h: %.table
	$(GPERF) $< --output $@

lib/ libsyslog-ng: lib/libsyslog-ng.la
if IVYKIS_INTERNAL
lib/ivykis/ ivykis: lib/ivykis/src/libivykis.la
endif

.PHONY: lib/ libsyslog-ng ivykis lib/ivykis/

include lib/tests/Makefile.am
