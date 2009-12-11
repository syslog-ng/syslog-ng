YFLAGS=@YFLAGS@

if ENABLE_ENV_WRAPPER
libexec_PROGRAMS = syslog-ng
sbin_PROGRAMS = syslog-ng-wrapper
else
sbin_PROGRAMS = syslog-ng
endif

bin_PROGRAMS = pdbtool

noinst_LIBRARIES = libsyslog-ng.a

syslog_ng_SOURCES = main.c
syslog_ng_LDADD = libsyslog-ng.a @DEPS_LIBS@
syslog_ng_wrapper_SOURCES = wrapper.c
pdbtool_SOURCES = pdbtool.c
pdbtool_LDADD = libsyslog-ng.a @BASE_LIBS@ @GLIB_LIBS@ @EVTLOG_LIBS@

libsyslog_ng_a_SOURCES = \
	misc.c misc.h utils.c utils.h messages.c messages.h syslog-names.c syslog-names.h \
	cfg.c cfg.h filter.c filter.h \
	logmsg.c logmsg.h logstamp.c logstamp.h \
	nvtable.c nvtable.h \
	logpipe.c logpipe.h logsource.c logsource.h driver.c driver.h sgroup.c sgroup.h \
	dgroup.c dgroup.h center.c center.h templates.c templates.h \
	logreader.c logreader.h logwriter.c logwriter.h \
	affile.c affile.h afstreams.c afstreams.h afuser.c afuser.h afprog.c afprog.h \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h afinter.c afinter.h \
	children.c children.h stats.c stats.h \
	gsockaddr.c gsockaddr.h gsocket.c gsocket.h \
	logtransport.c logtransport.h \
	logproto.c logproto.h \
	memtrace.c memtrace.h \
	dnscache.c dnscache.h serialize.c serialize.h apphook.c apphook.h atomic.h \
	radix.c radix.h \
	logpatterns.c logpatterns.h \
	cfg-lex.l cfg-grammar.y \
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
	logqueue.c logqueue.h afsql.c afsql.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h

cfg-lex.c: cfg-grammar.c

cfg-grammar.h: cfg-grammar.y

cfg-grammar.c: cfg-grammar.y

EXTRA_DIST = cfg-grammar.h cfg-lex.c cfg-grammar.c syslog-ng.h

MAINTAINER_CLEAN_FILES = cfg-lex.c cfg-grammar.c cfg-grammar.h

if ENABLE_ENV_WRAPPER

install-exec-hook:
	mv $(DESTDIR)/$(sbindir)/syslog-ng-wrapper $(DESTDIR)/$(sbindir)/syslog-ng
else

install-exec-hook:
	echo Nothing to do!
endif
