plugindir = @plugindir@
AM_CPPFLAGS = -I$(top_srcdir)/src -I..
export top_srcdir

plugin_LTLIBRARIES := libafsocket-notls.la
libafsocket_notls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c
libafsocket_notls_la_CPPFLAGS = $(AM_CPPFLAGS)
libafsocket_notls_la_LIBADD = $(LIBNET_LIBS) $(LIBWRAP_LIBS)

if ENABLE_SSL
plugin_LTLIBRARIES += libafsocket-tls.la
libafsocket_tls_la_SOURCES = \
	afsocket.c afsocket.h afunix.c afunix.h afinet.c afinet.h \
	tlscontext.c tlscontext.h tlstransport.c tlstransport.h \
	afsocket-grammar.y afsocket-parser.c afsocket-parser.h afsocket-plugin.c
libafsocket_tls_la_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SSL=1
libafsocket_tls_la_LIBADD = $(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS) $(LIBWRAP_LIBS)
endif


if ENABLE_SSL
install-exec-hook:
	ln -sf libafsocket-tls.so $(DESTDIR)/$(plugindir)/libafsocket.so
else
install-exec-hook:
	ln -sf libafsocket-notls.so $(DESTDIR)/$(plugindir)/libafsocket.so
endif

include $(top_srcdir)/build/lex-rules.am
