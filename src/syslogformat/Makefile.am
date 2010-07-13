plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libsyslogformat.la
libsyslogformat_la_SOURCES = \
	syslog-format.c syslog-format.h syslog-format-plugin.c

libsyslogformat_la_CPPFLAGS = $(AM_CPPFLAGS)

include $(top_srcdir)/build/lex-rules.am
