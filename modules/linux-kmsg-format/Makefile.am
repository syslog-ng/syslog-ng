SUBDIRS = . tests

moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := liblinux-kmsg-format.la
liblinux_kmsg_format_la_SOURCES = \
	linux-kmsg-format.c linux-kmsg-format.h linux-kmsg-format-plugin.c

liblinux_kmsg_format_la_CPPFLAGS = $(AM_CPPFLAGS)
liblinux_kmsg_format_la_LIBADD = $(MODULE_DEPS_LIBS)
liblinux_kmsg_format_la_LDFLAGS = $(MODULE_LDFLAGS)

include $(top_srcdir)/build/lex-rules.am
