plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
plugin_LTLIBRARIES = libafsocket.la

export top_srcdir

libafsocket_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c

.ym.y: $(srcdir)/$*.ym $(top_srcdir)/src/merge-grammar.pl $(top_srcdir)/src/cfg-grammar.y
	$(top_srcdir)/src/merge-grammar.pl $< > $@

.l.c:
	$(am__skiplex) $(SHELL) $(YLWRAP) $< $(LEX_OUTPUT_ROOT).c $*.c $(LEX_OUTPUT_ROOT).h $*.h -- $(LEXCOMPILE)

.l.h:
	$(am__skiplex) $(SHELL) $(YLWRAP) $< $(LEX_OUTPUT_ROOT).c $*.c $(LEX_OUTPUT_ROOT).h $*.h -- $(LEXCOMPILE)

.y.c:
	$(am__skipyacc) $(SHELL) $(YLWRAP) $< y.tab.c $@ y.tab.h $*.h y.output $*.output -- $(YACCCOMPILE)

.y.h:
	$(am__skipyacc) $(SHELL) $(YLWRAP) $< y.tab.c $@ y.tab.h $*.h y.output $*.output -- $(YACCCOMPILE)
