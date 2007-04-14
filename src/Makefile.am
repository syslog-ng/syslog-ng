YFLAGS=@YFLAGS@

sbin_PROGRAMS = syslog-ng
noinst_LIBRARIES = libsyslog-ng.a

syslog_ng_SOURCES = main.c
syslog_ng_LDADD = libsyslog-ng.a

libsyslog_ng_a_SOURCES = \
	misc.c misc.h utils.c utils.h messages.c messages.h syslog-names.c syslog-names.h \
	cfg.c cfg.h cfg-lex.l cfg-grammar.y macros.c macros.h filter.c filter.h \
	logmsg.c logmsg.h \
	logpipe.c logpipe.h logsource.c logsource.h driver.c driver.h sgroup.c sgroup.h \
	dgroup.c dgroup.h center.c center.h templates.c templates.h \
	logreader.c logreader.h logwriter.c logwriter.h \
	affile.c affile.h afstreams.c afstreams.h afuser.c afuser.h afprog.c afprog.h \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h afinter.c afinter.h \
	children.c children.h stats.c stats.h \
	gsockaddr.c gsockaddr.h fdwrite.c fdwrite.h fdread.c fdread.h memtrace.c memtrace.h \
	dnscache.c dnscache.h

cfg-lex.c: cfg-grammar.c

cfg-grammar.h: cfg-grammar.y

cfg-grammar.c: cfg-grammar.y

EXTRA_DIST = cfg-grammar.h cfg-lex.c cfg-grammar.c syslog-ng.h

MAINTAINER_CLEAN_FILES = cfg-lex.c cfg-grammar.c cfg-grammar.h
