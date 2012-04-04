moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

if ENABLE_SYSTEMD
if WITH_LIBSYSTEMD
SYSTEMD_SOURCES =
else
SYSTEMD_SOURCES = sd-daemon.c sd-daemon.h
endif
else
SYSTEMD_SOURCES =
endif

module_LTLIBRARIES := libafsocket-notls.la
noinst_DATA = libafsocket.la libafsocket-static.la
libafsocket_notls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c \
	$(SYSTEMD_SOURCES)
libafsocket_notls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)
libafsocket_notls_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_notls_la_LDFLAGS = $(MODULE_LDFLAGS)
if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES := libafsocket-notls-static.la
libafsocket_notls_static_la_SOURCES = $(libafsocket_notls_la_SOURCES)
libafsocket_notls_static_la_CPPFLAGS = $(libafsocket_notls_la_CPPFLAGS) -DSTATIC
libafsocket_notls_static_la_LIBADD = $(libafsocket_notls_la_LIBADD)
libafsocket_notls_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif

if ENABLE_SSL
module_LTLIBRARIES += libafsocket-tls.la
libafsocket_tls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c \
	$(SYSTEMD_SOURCES)
libafsocket_tls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS) -DENABLE_SSL=1
libafsocket_tls_la_LIBADD = $(MODULE_DEPS_LIBS) $(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_tls_la_LDFLAGS = $(MODULE_LDFLAGS)
if ENABLE_STATIC_MODULES
noinst_LTLIBRARIES += libafsocket-tls-static.la
libafsocket_tls_static_la_SOURCES = $(libafsocket_tls_la_SOURCES)
libafsocket_tls_static_la_CPPFLAGS = $(libafsocket_tls_la_CPPFLAGS) -DSTATIC
libafsocket_tls_static_la_LIBADD = $(libafsocket_tls_la_LIBADD)
libafsocket_tls_static_la_LDFLAGS = $(STATIC_MODULE_LDFLAGS)
endif
endif

BUILT_SOURCES = afsocket-grammar.y afsocket-grammar.c afsocket-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsocket-grammar.ym

if ENABLE_SSL
TLSSUFF=-tls
else
TLSSUFF=-notls
endif

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	ln -sf libafsocket$(TLSSUFF)$(BB_SHREXT) $(DESTDIR)$(moduledir)/libafsocket$(BB_SHREXT)

libafsocket.la: libafsocket$(TLSSUFF).la
	ln -sf libafsocket$(TLSSUFF).la libafsocket.la
	(cd .libs && rm -f libafsocket.a && ln -s libafsocket$(TLSSUFF).a libafsocket.a)

libafsocket-static.la: libafsocket$(TLSSUFF)-static.la
	ln -sf libafsocket$(TLSSUFF)-static.la libafsocket-static.la
	(cd .libs && rm -f libafsocket-static.a && ln -s libafsocket$(TLSSUFF)-static.a libafsocket-static.a)

uninstall-hook:
	rm -f $(DESTDIR)$(moduledir)/libafsocket$(BB_SHREXT)

CLEANFILES = libafsocket.la

include $(top_srcdir)/build/lex-rules.am
