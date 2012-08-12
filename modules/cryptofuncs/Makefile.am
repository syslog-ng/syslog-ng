moduledir = @moduledir@
export top_srcdir

AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libcryptofuncs.la

libcryptofuncs_la_SOURCES = cryptofuncs.c
libcryptofuncs_la_LIBADD = $(MODULE_DEPS_LIBS) ../../lib/libsyslog-ng-crypto.la $(OPENSSL_LIBS)
libcryptofuncs_la_LDFLAGS = $(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)
