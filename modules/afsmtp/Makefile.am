moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libafsmtp.la

export top_srcdir

if ENABLE_SMTP

libafsmtp_la_CFLAGS = $(LIBESMTP_CFLAGS) $(IVYKIS_CFLAGS) $(EVENTLOG_CFLAGS) $(SYSLOG_NG_CFLAGS)
libafsmtp_la_SOURCES = afsmtp-grammar.y afsmtp.c afsmtp.h afsmtp-parser.c afsmtp-parser.h
libafsmtp_la_LIBADD = $(LIBESMTP_LIBS) $(IVYKIS_LIBS) $(EVENTLOG_LIBS) $(SYSLOG_NG_LIBS)
libafsmtp_la_LDFLAGS = -avoid-version

endif

BUILT_SOURCES = afsmtp-grammar.y afsmtp-grammar.c afsmtp-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsmtp-grammar.ym

include $(top_srcdir)/build/lex-rules.am
