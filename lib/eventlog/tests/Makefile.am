AM_CPPFLAGS=-I$(top_srcdir)/src

noinst_PROGRAMS = evtrec evtfmt evtsyslog evtsyslog-macros

evtrec_SOURCES = evtrec.c
evtrec_LDADD = ../src/libevtlog.la

evtfmt_SOURCES = evtfmt.c
evtfmt_LDADD = ../src/libevtlog.la

evtsyslog_SOURCES = evtsyslog.c
evtsyslog_LDADD = ../src/libevtlog.la

evtsyslog_macros_SOURCES = evtsyslog.c
evtsyslog_macros_CFLAGS = -DEVENTLOG_SYSLOG_MACROS=1
evtsyslog_macros_LDADD = ../src/libevtlog.la
