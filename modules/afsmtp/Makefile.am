module_LTLIBRARIES			= libafsmtp.la
export top_srcdir

libafsmtp_la_CFLAGS	=	$(LIBESMTP_CFLAGS)
libafsmtp_la_SOURCES	=	\
	afsmtp-grammar.y		\
	afsmtp.c			\
	afsmtp.h			\
	afsmtp-parser.c		\
	afsmtp-parser.h

libafsmtp_la_LIBADD	=	$(LIBESMTP_LIBS) $(MODULE_DEPS_LIBS) $(CRYPTO_LIBS)
libafsmtp_la_LDFLAGS	=	$(MODULE_LDFLAGS)
libafsmtp_la_DEPENDENCIES=	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS)

include $(top_srcdir)/build/lex-rules.am

# divert install/uninstall targets to avoid recursing into $(SUBDIRS)
install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am

BUILT_SOURCES	= afsmtp-grammar.y afsmtp-grammar.c afsmtp-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsmtp-grammar.ym
