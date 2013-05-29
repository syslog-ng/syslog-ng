YFLAGS=@YFLAGS@

export top_srcdir

bin_PROGRAMS = logchksign
logchksign_SOURCES = logchksign.c
logchksign_LDADD = $(OPENSSL_LIBS)

CLEAN_FILES = $(BUILT_SOURCES)

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)

install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am

include $(top_srcdir)/build/lex-rules.am
