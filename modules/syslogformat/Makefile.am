moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libsyslogformat.la
libsyslogformat_la_SOURCES = \
	syslog-format.c syslog-format.h syslog-format-plugin.c

libsyslogformat_la_CPPFLAGS = $(AM_CPPFLAGS)
libsyslogformat_la_LIBADD = $(MODULE_DEPS_LIBS)
libsyslogformat_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libsyslogformat-static.la
libsyslogformat_static_la_SOURCES = $(libsyslogformat_la_SOURCES)
libsyslogformat_static_la_CPPFLAGS = $(libsyslogformat_la_CPPFLAGS) -DSTATIC
libsyslogformat_static_la_LIBADD =
libsyslogformat_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif

include $(top_srcdir)/build/lex-rules.am
