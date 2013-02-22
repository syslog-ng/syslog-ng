moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libafsocket-notls.la
noinst_DATA = libafsocket.la
libafsocket_notls_la_SOURCES = \
	afsocket.c		\
	afsocket.h		\
	afsocket-source.c	\
	afsocket-source.h	\
	afsocket-dest.c		\
	afsocket-dest.h		\
	afinet.c		\
	afinet.h		\
	afinet-source.c		\
	afinet-source.h		\
	afinet-dest.c		\
	afinet-dest.h		\
	afunix-source.c		\
	afunix-source.h		\
	afunix-dest.c		\
	afunix-dest.h		\
	afsocket-grammar.y	\
	afsocket-parser.c	\
	afsocket-parser.h	\
	afsocket-plugin.c

libafsocket_notls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)
libafsocket_notls_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_notls_la_LDFLAGS = $(MODULE_LDFLAGS)

if ENABLE_SSL
module_LTLIBRARIES += libafsocket-tls.la
libafsocket_tls_la_SOURCES = $(libafsocket_notls_la_SOURCES)
libafsocket_tls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS) -DBUILD_WITH_SSL=1

libafsocket_tls_la_LIBADD = $(MODULE_DEPS_LIBS) $(CRYPTO_LIBS) $(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_tls_la_LDFLAGS = $(MODULE_LDFLAGS)
endif

BUILT_SOURCES = afsocket-grammar.y afsocket-grammar.c afsocket-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsocket-grammar.ym

if ENABLE_SSL
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	ln -sf libafsocket-tls.so $(DESTDIR)$(moduledir)/libafsocket.so

libafsocket.la: libafsocket-tls.la
	ln -sf libafsocket-tls.la libafsocket.la

else
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	ln -sf libafsocket-notls.so $(DESTDIR)$(moduledir)/libafsocket.so


libafsocket.la: libafsocket-tls.la
	ln -sf libafsocket-notls.la libafsocket.la

endif

uninstall-hook:
	rm -f $(DESTDIR)$(moduledir)/libafsocket.so

CLEANFILES = libafsocket.la

include $(top_srcdir)/build/lex-rules.am
