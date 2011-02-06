SUBDIRS = ivykis

YFLAGS=@YFLAGS@

export top_srcdir

lib_LTLIBRARIES = libsyslog-ng.la
libsyslog_ng_la_LIBADD = @CORE_DEPS_LIBS@
libsyslog_ng_la_LDFLAGS = -no-undefined

libsyslog_ng_la_SOURCES = \
	misc.c misc.h utils.c utils.h messages.c messages.h syslog-names.c syslog-names.h \
	mainloop.c mainloop.h \
	cfg.c cfg.h filter.c filter.h \
	logmsg.c logmsg.h logstamp.c logstamp.h \
	msg-format.c msg-format.h \
	nvtable.c nvtable.h \
	logpipe.c logpipe.h logsource.c logsource.h driver.c driver.h sgroup.c sgroup.h \
	dgroup.c dgroup.h center.c center.h templates.c templates.h \
	logreader.c logreader.h logwriter.c logwriter.h \
	afinter.c afinter.h \
	children.c children.h stats.c stats.h \
	gsockaddr.c gsockaddr.h gsocket.c gsocket.h \
	logtransport.c logtransport.h \
	logproto.c logproto.h \
	memtrace.c memtrace.h \
	persist-state.c persist-state.h \
	dnscache.c dnscache.h serialize.c serialize.h apphook.c apphook.h atomic.h \
	timeutils.c timeutils.h \
	str-format.c str-format.h \
	tags.c tags.h \
	alarms.c alarms.h \
	logparser.c logparser.h \
	logprocess.c logprocess.h \
	logmpx.c logmpx.h \
	logrewrite.c logrewrite.h \
	logmatcher.c logmatcher.h \
	gprocess.c gprocess.h \
	globals.c \
	control.c control.h \
	compat.c compat.h \
	logqueue.c logqueue.h \
	cfg-lexer.c cfg-lexer.h cfg-lex.l \
	cfg-parser.c cfg-parser.h cfg-grammar.y \
	plugin.c plugin.h \
	filter-expr-grammar.y filter-expr-parser.c filter-expr-parser.h \
	block-ref-grammar.y block-ref-parser.c block-ref-parser.h \
	pragma-grammar.y pragma-parser.h pragma-parser.c \
	parser-expr-grammar.y parser-expr-parser.c parser-expr-parser.h \
	rewrite-expr-grammar.y rewrite-expr-parser.c rewrite-expr-parser.h

BUILT_SOURCES = cfg-lex.c cfg-lex.h cfg-grammar.c cfg-grammar.h \
	filter-expr-grammar.y filter-expr-grammar.c filter-expr-grammar.h \
	block-ref-grammar.y block-ref-grammar.c block-ref-grammar.h \
	pragma-grammar.y pragma-grammar.h pragma-grammar.c \
	parser-expr-grammar.y parser-expr-grammar.c parser-expr-grammar.h \
	rewrite-expr-grammar.y rewrite-expr-grammar.c rewrite-expr-grammar.h

EXTRA_DIST = $(BUILT_SOURCES) \
	filter-expr-grammar.ym block-ref-grammar.ym pragma-grammar.ym parser-expr-grammar.ym rewrite-expr-grammar.ym \
	syslog-ng.h

CLEAN_FILES = $(BUILT_SOURCES)

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)
install: install-am

uninstall: uninstall-am
include $(top_srcdir)/build/lex-rules.am
