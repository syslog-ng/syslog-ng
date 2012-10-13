AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/dbparser -I..
AM_CFLAGS = @CFLAGS_NOWARN_POINTER_SIGN@

export top_srcdir

bin_PROGRAMS = pdbtool
pdbtool_SOURCES = pdbtool.c
pdbtool_CPPFLAGS = $(AM_CPPFLAGS)
pdbtool_LDADD = ../libsyslog-ng-patterndb.a ../../../lib/libsyslog-ng.la ../../../lib/libsyslog-ng-crypto.la @TOOL_DEPS_LIBS@

include $(top_srcdir)/build/lex-rules.am
