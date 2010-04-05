plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
plugin_LTLIBRARIES = libdummy.la

export top_srcdir

libdummy_la_SOURCES = dummy-grammar.y dummy.c dummy-parser.c dummy-parser.h

.ym.y: $(srcdir)/$*.ym $(top_srcdir)/src/merge-grammar.pl $(top_srcdir)/src/cfg-grammar.y
	$(top_srcdir)/src/merge-grammar.pl $< > $@

MAINTAINER_CLEAN_FILES = dummy-grammar.c dummy-grammar.h

.y.c:
	$(am__skipyacc) $(SHELL) $(YLWRAP) $< y.tab.c $@ y.tab.h $*.h y.output $*.output -- $(YACCCOMPILE)

.y.h:
	$(am__skipyacc) $(SHELL) $(YLWRAP) $< y.tab.c $@ y.tab.h $*.h y.output $*.output -- $(YACCCOMPILE)
