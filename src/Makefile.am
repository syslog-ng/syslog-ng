YFLAGS=@YFLAGS@

sbin_PROGRAMS = syslog-ng

syslog_ng_SOURCES = \
	main.c misc.c utils.c messages.c gsockaddr.c syslog-names.c macros.c \
	logmsg.c \
	cfg.c logpipe.c driver.c sgroup.c dgroup.c center.c filter.c \
	logreader.c logwriter.c \
	affile.c afunix.c afinet.c afsocket.c afstreams.c afuser.c afprog.c afinter.c \
	cfg-lex.l cfg-grammar.y fdwrite.c fdread.c memtrace.c templates.c children.c

cfg-lex.l: cfg-grammar.h

cfg-grammar.h: cfg-grammar.c

EXTRA_DIST = affile.h afinet.h afinter.h afprog.h afsocket.h afstreams.h \
	afunix.h afuser.h center.h cfg-grammar.h cfg.h dgroup.h driver.h filter.h \
	gsockaddr.h logmsg.h logpipe.h logreader.h logwriter.h macros.h messages.h \
	misc.h sgroup.h syslog-names.h \
	syslog-ng.h utils.h memtrace.h templates.h children.h fdread.h fdwrite.h

