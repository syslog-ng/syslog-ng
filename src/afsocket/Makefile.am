plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
plugin_LTLIBRARIES = libafsocket.la

export top_srcdir

libafsocket_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c

afsocket-grammar.y: afsocket-grammar.y.in ../merge-grammar.pl ../cfg-grammar.y
	$(top_srcdir)/src/merge-grammar.pl $(srcdir)/afsocket-grammar.y.in > $@
