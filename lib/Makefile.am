moduledir = @moduledir@
pkgincludedir = @includedir@/syslog-ng

template_headers = \
	template/templates.h

template_sources = \
	template/templates.c

YFLAGS=@YFLAGS@

export top_srcdir

lib_LTLIBRARIES = libsyslog-ng.la
libsyslog_ng_la_LIBADD = @CORE_DEPS_LIBS@ @OPENSSL_LIBS@ $(PCRE_LIBS) -ljson
libsyslog_ng_la_LDFLAGS = -no-undefined -release @LIBSYSLOGNG_VERSION@
libsyslog_ng_la_CFLAGS = -DLIBSYSLOG_NG -ggdb

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
pkginclude_HEADERS = 		\
	afinter.h		\
	apphook.h		\
	atomic.h		\
	alarms.h		\
	block-ref-parser.h	\
  cache.h      \
	center.h		\
	cfg.h                   \
	cfg-grammar.h           \
	cfg-lexer.h		\
	cfg-parser.h		\
	children.h		\
	compat.h		\
	control.h		\
	control_server.h \
	crypto.h		\
	dgroup.h		\
	dnscache.h		\
	driver.h		\
	filter-expr-parser.h	\
	filter.h		\
	gsocket.h		\
	gsockaddr.h		\
  hostname.h  \
	logmatcher.h		\
	logmpx.h		\
	logmsg.h		\
	logparser.h		\
	logpipe.h		\
	logprocess.h		\
	logproto.h		\
	logqueue-fifo.h		\
	logqueue.h		\
	logreader.h		\
	logrewrite.h		\
	logsource.h		\
	logstamp.h		\
	logtransport.h		\
  logthrdestdrv.h \
	logwriter.h		\
	mainloop.h		\
  mainloop-call.h \
  mainloop-io-worker.h \
  mainloop-worker.h \
	memtrace.h		\
	messages.h		\
  message_ids.h \
	misc.h			\
	msg-format.h		\
	nvtable.h		\
	parser-expr-parser.h	\
	persist-state.h		\
	plugin.h		\
	pragma-parser.h		\
	rewrite-expr-parser.h	\
	scratch-buffers.h	\
	serialize.h		\
	sgroup.h		\
	state.h			\
	stats.h			\
	str-format.h		\
	syslog-names.h		\
	syslog-ng.h		\
	tags.h			\
  thread-utils.h \
	timeutils.h		\
	tls-support.h		\
	tlscontext.h  		\
	tlstransport.h		\
  tlswincrypt.h   \
	type-hinting.h	\
	utils.h			\
	uuid.h			\
	value-pairs.h		\
	versioning.h	\
	vptransform.h	\
	reloc.h	\
	ringbuffer.h	\
	bookmark.h	\
	ack_tracker.h \
	poll-events.h \
	poll-fd-events.h \
	parse-number.h \
  $(template_headers)

if !IS_WINDOWS
pkginclude_HEADERS += gprocess.h
endif

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
libsyslog_ng_crypto_la_SOURCES = \
	crypto.c		\
	tlscontext.c		\
	tlstransport.c

# this is intentionally formatted so conflicts are less likely to arise. one name in every line.
libsyslog_ng_la_SOURCES = \
	afinter.c		\
	apphook.c		\
	alarms.c		\
	block-ref-parser.c	\
  cache.c     \
	center.c		\
	cfg.c			\
	cfg-lexer.c		\
	cfg-parser.c		\
	children.c		\
	compat.c		\
	control.c		\
	control_server.c \
	dgroup.c		\
	dnscache.c		\
	driver.c		\
	filter.c		\
	filter-expr-parser.c	\
	globals.c		\
	gsocket.c		\
	gsockaddr.c		\
	hostname_common.c		\
	logmatcher.c		\
	logmpx.c		\
	logmsg.c		\
	logparser.c		\
	logpipe.c		\
	logprocess.c		\
	logproto.c		\
	logqueue.c		\
	logqueue-fifo.c		\
	logreader.c		\
	logrewrite.c		\
	logsource.c		\
	logstamp.c		\
	logtransport.c		\
  logthrdestdrv.c \
	logwriter.c		\
	mainloop.c		\
  mainloop-call.c \
  mainloop-io-worker.c \
  mainloop-worker.c \
	memtrace.c		\
	messages.c		\
	misc.c			\
	msg-format.c		\
	nvtable.c		\
	parser-expr-parser.c	\
	persist-state.c		\
	plugin.c		\
	pragma-parser.c		\
	rewrite-expr-parser.c	\
	scratch-buffers.c	\
	serialize.c		\
	sgroup.c		\
	state.c			\
	stats.c			\
	str-format.c		\
	syslog-names.c		\
	tags.c			\
	timeutils.c		\
	type-hinting.c	\
	utils.c			\
	uuid.c			\
	value-pairs.c		\
	versioning.c		\
	vptransform.c		\
	cfg-lex.l		\
	cfg-grammar.y		\
	filter-expr-grammar.y	\
	block-ref-grammar.y	\
	pragma-grammar.y	\
	parser-expr-grammar.y	\
	rewrite-expr-grammar.y	\
	reloc.c                 \
	ringbuffer.c		\
	early_ack_tracker.c	\
	late_ack_tracker.c	\
	poll-events.c		\
	poll-fd-events.c	\
	parse-number.c		\
	$(libsyslog_ng_crypto_la_SOURCES) \
  $(template_sources)

if !IS_WINDOWS
libsyslog_ng_la_SOURCES += gprocess.c \
													 control_server_posix.c \
                           hostname_unix.c
else
libsyslog_ng_la_SOURCES += hostname_win.c \
                           tlswincrypt.c  \
                           control_server_windows.c

endif



# each line with closely related files (e.g. the ones generated from the same source)
BUILT_SOURCES = cfg-lex.c cfg-lex.h						\
	cfg-grammar.c cfg-grammar.h						\
	filter-expr-grammar.y filter-expr-grammar.c filter-expr-grammar.h	\
	block-ref-grammar.y block-ref-grammar.c block-ref-grammar.h		\
	pragma-grammar.y pragma-grammar.h pragma-grammar.c			\
	parser-expr-grammar.y parser-expr-grammar.c parser-expr-grammar.h	\
	rewrite-expr-grammar.y rewrite-expr-grammar.c rewrite-expr-grammar.h

EXTRA_DIST = $(BUILT_SOURCES) \
	filter-expr-grammar.ym		\
	block-ref-grammar.ym		\
	pragma-grammar.ym		\
	parser-expr-grammar.ym		\
	rewrite-expr-grammar.ym		\
	merge-grammar.pl		\
	libsyslogng.rc			\
	libsyslogng.rc.in		\
	makefile.msc

libsyslogng.rc: libsyslogng.rc.in
	$(top_builddir)/config.status --file $@:$<

CLEAN_FILES = $(BUILT_SOURCES)

TESTS = $(check_PROGRAMS)

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)

install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am

include $(top_srcdir)/build/lex-rules.am
