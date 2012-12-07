
SUBDIRS = @LIBRABBITMQ_SUBDIRS@
DIST_SUBDIRS = rabbitmq-c

moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libafamqp.la

export top_srcdir

if ENABLE_AMQP

libafamqp_la_CFLAGS = $(LIBRABBITMQ_CFLAGS)
libafamqp_la_SOURCES = afamqp-grammar.y afamqp.c afamqp.h afamqp-parser.c afamqp-parser.h
libafamqp_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBRABBITMQ_LIBS)
libafamqp_la_LDFLAGS = $(MODULE_LDFLAGS)

endif

BUILT_SOURCES = afamqp-grammar.y afamqp-grammar.c afamqp-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afamqp-grammar.ym rabbitmq-c/configure.gnu

include $(top_srcdir)/build/lex-rules.am

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)

install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am

check:
	echo "Make check disabled, since it requires a newer glib"
