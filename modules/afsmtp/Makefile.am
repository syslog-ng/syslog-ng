moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib

export top_srcdir

if ENABLE_SMTP
module_LTLIBRARIES = libafsmtp.la

libafsmtp_la_CFLAGS = $(LIBESMTP_CFLAGS)
libafsmtp_la_SOURCES = afsmtp-grammar.y afsmtp.c afsmtp.h afsmtp-parser.c afsmtp-parser.h
libafsmtp_la_LIBADD = $(LIBESMTP_LIBS) $(MODULE_DEPS_LIBS)
libafsmtp_la_LDFLAGS = $(MODULE_LDFLAGS)

endif

BUILT_SOURCES = afsmtp-grammar.y afsmtp-grammar.c afsmtp-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsmtp-grammar.ym

include $(top_srcdir)/build/lex-rules.am
