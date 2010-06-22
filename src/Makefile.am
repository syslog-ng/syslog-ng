SUBDIRS = . afsocket afsql afstreams affile confgen dummy
YFLAGS=@YFLAGS@

export top_srcdir


if ENABLE_ENV_WRAPPER
libexec_PROGRAMS = syslog-ng
sbin_PROGRAMS = syslog-ng-wrapper syslog-ng-ctl
else
sbin_PROGRAMS = syslog-ng syslog-ng-ctl
endif

bin_PROGRAMS = pdbtool

noinst_LIBRARIES = libsyslog-ng.a

syslog_ng_SOURCES = main.c
syslog_ng_LDADD = libsyslog-ng.a @DEPS_LIBS@ globals.o
syslog_ng_wrapper_SOURCES = wrapper.c
pdbtool_SOURCES = pdbtool.c
pdbtool_LDADD = libsyslog-ng.a @BASE_LIBS@ @GLIB_LIBS@ @EVTLOG_LIBS@

syslog_ng_ctl_SOURCES = syslog-ng-ctl.c gsockaddr.c  gsockaddr.h  gsocket.c    gsocket.h
syslog_ng_ctl_LDADD = @DEPS_LIBS@

libsyslog_ng_a_SOURCES = \
	misc.c misc.h utils.c utils.h messages.c messages.h syslog-names.c syslog-names.h \
	cfg.c cfg.h filter.c filter.h \
	logmsg.c logmsg.h logstamp.c logstamp.h parse-syslog.c parse-syslog.h \
	nvtable.c nvtable.h \
	logpipe.c logpipe.h logsource.c logsource.h driver.c driver.h sgroup.c sgroup.h \
	dgroup.c dgroup.h center.c center.h templates.c templates.h \
	logreader.c logreader.h logwriter.c logwriter.h \
	afuser.c afuser.h afprog.c afprog.h \
	afinter.c afinter.h \
	children.c children.h stats.c stats.h \
	gsockaddr.c gsockaddr.h gsocket.c gsocket.h \
	logtransport.c logtransport.h \
	logproto.c logproto.h \
	memtrace.c memtrace.h \
	dnscache.c dnscache.h serialize.c serialize.h apphook.c apphook.h atomic.h \
	radix.c radix.h \
	logpatterns.c logpatterns.h \
	timeutils.c timeutils.h \
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
	pragma-grammar.y pragma-parser.h pragma-parser.c

EXTRA_DIST = cfg-grammar.h cfg-grammar.c cfg-lex.h cfg-lex.c syslog-ng.h radix-find.c filter-expr-grammar.c filter-expr-gammar.h pragma-grammar.c pragma-grammar.y

CLEANFILES = cfg-lex.c cfg-lex.h cfg-grammar.c cfg-grammar.h filter-expr-grammar.y filter-expr-grammar.c filter-expr-grammar.h pragma-grammar.y pragma-grammar.c pragma-grammar.h

if ENABLE_ENV_WRAPPER

install-exec-hook:
	mv $(DESTDIR)/$(sbindir)/syslog-ng-wrapper $(DESTDIR)/$(sbindir)/syslog-ng
else

install-exec-hook:
	echo Nothing to do!
endif

include $(top_srcdir)/build/lex-rules.am