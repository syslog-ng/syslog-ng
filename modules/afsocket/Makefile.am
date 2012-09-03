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
noinst_DATA = libafsocket.la
libafsocket_notls_la_SOURCES = \
	afsocket.c afsocket.h afinet.c afinet.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c

if !IS_WINDOWS
libafsocket_notls_la_SOURCES += \
	afunix.c afunix.h \
	$(SYSTEMD_SOURCES)
endif
libafsocket_notls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)
libafsocket_notls_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_notls_la_LDFLAGS = $(MODULE_LDFLAGS) -lpcre

if ENABLE_SSL
module_LTLIBRARIES += libafsocket-tls.la
libafsocket_tls_la_SOURCES = \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c \
	afsocket.c afsocket.h afinet.c afinet.h

if !IS_WINDOWS
libafsocket_tls_la_SOURCES += \
	afunix.c afunix.h $(SYSTEMD_SOURCES)
endif

libafsocket_tls_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS) -DENABLE_SSL=1
libafsocket_tls_la_LIBADD = $(MODULE_DEPS_LIBS) $(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
libafsocket_tls_la_LDFLAGS = $(MODULE_LDFLAGS) -lpcre
endif

BUILT_SOURCES = afsocket-grammar.y afsocket-grammar.c afsocket-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) afsocket-grammar.ym

if ENABLE_SSL
TLSSUFF=-tls
else
TLSSUFF=-notls
endif

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
if IS_WINDOWS
	cp $(DESTDIR)$(moduledir)/libafsocket$(TLSSUFF)$(BB_SHREXT) $(DESTDIR)$(moduledir)/libafsocket$(BB_SHREXT)
else
	ln -sf libafsocket$(TLSSUFF)$(BB_SHREXT) $(DESTDIR)$(moduledir)/libafsocket$(BB_SHREXT)
endif

libafsocket.la: libafsocket$(TLSSUFF).la
	ln -sf libafsocket$(TLSSUFF).la libafsocket.la
if IS_WINDOWS
	(cd .libs && rm -f libafsocket.dll.a && cp libafsocket$(TLSSUFF).dll.a libafsocket.dll.a)
	(cd .libs && rm -f libafsocket.dll && cp libafsocket$(TLSSUFF).dll libafsocket.dll)
else
	(cd .libs && rm -f libafsocket.a && ln -s libafsocket$(TLSSUFF).a libafsocket.a)
endif

uninstall-hook:
	rm -f $(DESTDIR)$(moduledir)/libafsocket$(BB_SHREXT)

CLEANFILES = libafsocket.la libafsocket-static.la

include $(top_srcdir)/build/lex-rules.am
