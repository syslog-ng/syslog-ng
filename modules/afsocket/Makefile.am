moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

if ENABLE_SYSTEMD
SYSTEMD_SOURCES = sd-daemon.c sd-daemon.h
else
SYSTEMD_SOURCES =
endif

module_LTLIBRARIES := libafsocket-notls.la
noinst_DATA = libafsocket.la
libafsocket_notls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c \
	$(SYSTEMD_SOURCES)
libafsocket_notls_la_CPPFLAGS = $(AM_CPPFLAGS)
libafsocket_notls_la_LIBADD = ../../lib/libsyslog-ng.la $(LIBNET_LIBS) $(LIBWRAP_LIBS)
libafsocket_notls_la_LDFLAGS = -avoid-version -module -no-undefined

if ENABLE_SSL
module_LTLIBRARIES += libafsocket-tls.la
libafsocket_tls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c \
	$(SYSTEMD_SOURCES)
libafsocket_tls_la_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SSL=1
libafsocket_tls_la_LIBADD = ../../lib/libsyslog-ng.la $(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS)
libafsocket_tls_la_LDFLAGS = -avoid-version -module -no-undefined
endif

BUILT_SOURCES = afsocket-grammar.y afsocket-grammar.c afsocket-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsocket-grammar.ym

if ENABLE_SSL
install-exec-hook:
	mkdir -p $(DESTDIR)$(moduledir) || true
	ln -sf libafsocket-tls.so $(DESTDIR)$(moduledir)/libafsocket.so

libafsocket.la: libafsocket-tls.la
	ln -sf libafsocket-tls.la libafsocket.la

else
install-exec-hook:
	mkdir -p $(DESTDIR)$(moduledir) || true
	ln -sf libafsocket-notls.so $(DESTDIR)$(moduledir)/libafsocket.so


libafsocket.la: libafsocket-tls.la
	ln -sf libafsocket-notls.la libafsocket.la

endif

uninstall-hook:
	rm -f $(DESTDIR)$(moduledir)/libafsocket.so

CLEANFILES = libafsocket.la

include $(top_srcdir)/build/lex-rules.am
