SUBDIRS = . tests
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libcryptofuncs.la
libcryptofuncs_la_SOURCES = cryptofuncs.c
libcryptofuncs_la_CPPFLAGS = $(AM_CPPFLAGS) $(OPENSSL_CFLAGS)
libcryptofuncs_la_LIBADD = $(MODULE_DEPS_LIBS) $(OPENSSL_LIBS)
libcryptofuncs_la_LDFLAGS = $(MODULE_LDFLAGS)
