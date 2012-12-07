moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libsyslogformat.la
libsyslogformat_la_SOURCES = \
	syslog-format.c		\
	syslog-format.h		\
	syslog-format-plugin.c	\
	syslog-parser-grammar.y	\
	syslog-parser-parser.c	\
	syslog-parser-parser.h	\
	syslog-parser.c		\
	syslog-parser.h

BUILT_SOURCES = syslog-parser-grammar.c syslog-parser-grammar.h
EXTRA_DIST = $(BUILT_SOURCES)

libsyslogformat_la_CPPFLAGS = $(AM_CPPFLAGS)
libsyslogformat_la_LIBADD = $(MODULE_DEPS_LIBS)
libsyslogformat_la_LDFLAGS = $(MODULE_LDFLAGS)


include $(top_srcdir)/build/lex-rules.am
