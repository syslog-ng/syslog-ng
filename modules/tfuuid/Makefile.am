moduledir = @moduledir@
export top_srcdir

AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libtfuuid.la

libtfuuid_la_SOURCES = tfuuid.c
libtfuuid_la_LIBADD = $(MODULE_DEPS_LIBS) ../../lib/libsyslog-ng-crypto.la
libtfuuid_la_LDFLAGS = $(MODULE_LDFLAGS)
