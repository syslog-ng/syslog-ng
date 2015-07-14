include modules/riak/libriack/Makefile.am

if ENABLE_RIAK

module_LTLIBRARIES			+=	\
	modules/riak/libriak.la

modules_riak_libriak_la_CFLAGS		=	    \
	-I$(top_srcdir)/modules/riak/libriack/lib   \
	-I$(top_builddir)/modules/riak/libriack/lib \
	-I$(top_srcdir)/modules/riak		    \
	-I$(top_builddir)/modules/riak
modules_riak_libriak_la_SOURCES		=	\
	modules/riak/riak-grammar.y 		\
	modules/riak/riak.c			\
	modules/riak/riak.h			\
	modules/riak/riak-parser.c		\
	modules/riak/riak-parser.h
modules_riak_libriak_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) 		\
	modules/riak/libriack/lib/libriack.la
modules_riak_libriak_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_riak_libriak_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS) \
	modules/riak/libriack/lib/libriack.la

modules/riak modules/riak/ mod-riak: \
	modules/riak/libriak.la
else
modules/riak modules/riak/ mod-riak:
endif

BUILT_SOURCES				+=	\
	modules/riak/riak-grammar.y		\
	modules/riak/riak-grammar.c		\
	modules/riak/riak-grammar.h
EXTRA_DIST				+=	\
	modules/riak/riak-grammar.ym \
	modules/riak/libriack/configure.ac

.PHONY: modules/riak/ mod-riak
