SUBDIRS = @LIBMONGO_SUBDIRS@
DIST_SUBDIRS = libmongo-client

moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib

export top_srcdir

if ENABLE_MONGODB
module_LTLIBRARIES = libafmongodb.la

libafmongodb_la_CFLAGS = $(LIBMONGO_CFLAGS)
libafmongodb_la_SOURCES = afmongodb-grammar.y afmongodb.c afmongodb.h afmongodb-parser.c afmongodb-parser.h
libafmongodb_la_LIBADD = $(MODULE_DEPS_LIBS) $(CRYPTO_LIBS) $(LIBMONGO_LIBS)
libafmongodb_la_LDFLAGS = $(MODULE_LDFLAGS)

endif

BUILT_SOURCES = afmongodb-grammar.y afmongodb-grammar.c afmongodb-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afmongodb-grammar.ym libmongo-client/configure.gnu

include $(top_srcdir)/build/lex-rules.am

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)

install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am

check:
	echo "Make check disabled, since it requires a newer glib"
