module_LTLIBRARIES				+= modules/afsocket/libafsocket-notls.la
noinst_DATA					+= modules/afsocket/libafsocket.la
modules_afsocket_libafsocket_notls_la_SOURCES	=	\
	modules/afsocket/afsocket.c			\
	modules/afsocket/afsocket.h			\
	modules/afsocket/afsocket-source.c		\
	modules/afsocket/afsocket-source.h		\
	modules/afsocket/afsocket-dest.c		\
	modules/afsocket/afsocket-dest.h		\
	modules/afsocket/socket-options.c        	\
	modules/afsocket/socket-options.h		\
	modules/afsocket/transport-mapper.c		\
	modules/afsocket/transport-mapper.h		\
	modules/afsocket/afinet.c			\
	modules/afsocket/afinet.h			\
	modules/afsocket/afinet-source.c		\
	modules/afsocket/afinet-source.h		\
	modules/afsocket/afinet-dest.c			\
	modules/afsocket/afinet-dest.h			\
	modules/afsocket/socket-options-inet.c		\
	modules/afsocket/socket-options-inet.h   	\
	modules/afsocket/transport-mapper-inet.c 	\
	modules/afsocket/transport-mapper-inet.h 	\
	modules/afsocket/afunix-source.c		\
	modules/afsocket/afunix-source.h		\
	modules/afsocket/afunix-dest.c			\
	modules/afsocket/afunix-dest.h			\
	modules/afsocket/transport-mapper-unix.c 	\
	modules/afsocket/transport-mapper-unix.h 	\
	modules/afsocket/transport-unix-socket.c	\
	modules/afsocket/transport-unix-socket.h	\
	modules/afsocket/unix-credentials.c		\
	modules/afsocket/unix-credentials.h		\
	modules/afsocket/afsocket-grammar.y		\
	modules/afsocket/afsocket-parser.c		\
	modules/afsocket/afsocket-parser.h		\
	modules/afsocket/afsocket-plugin.c      \
	modules/afsocket/systemd-syslog-source.h       \
	modules/afsocket/systemd-syslog-source.c

modules_afsocket_libafsocket_notls_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)	\
	-I${top_srcdir}/modules/afsocket		\
	-I${top_builddir}/modules/afsocket
modules_afsocket_libafsocket_notls_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(LIBNET_LIBS)		\
	$(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
modules_afsocket_libafsocket_notls_la_LDFLAGS	  =	\
	$(MODULE_LDFLAGS)
modules_afsocket_libafsocket_notls_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

if ENABLE_SSL
module_LTLIBRARIES				+= modules/afsocket/libafsocket-tls.la
modules_afsocket_libafsocket_tls_la_SOURCES	=	\
	$(modules_afsocket_libafsocket_notls_la_SOURCES)
modules_afsocket_libafsocket_tls_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)	\
	-I${top_srcdir}/modules/afsocket		\
	-I${top_builddir}/modules/afsocket		\
	-DBUILD_WITH_SSL=1

modules_afsocket_libafsocket_tls_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS) 		\
	$(OPENSSL_LIBS) $(ZLIB_LIBS) $(LIBNET_LIBS)	\
	$(LIBWRAP_LIBS) $(libsystemd_daemon_LIBS)
modules_afsocket_libafsocket_tls_la_DEPENDENCIES= 	\
	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS)
modules_afsocket_libafsocket_tls_la_LDFLAGS	= 	\
	$(MODULE_LDFLAGS)

modules/afsocket modules/afsocket/ mod-afsocket mod-socket: \
	modules/afsocket/libafsocket-notls.la \
	modules/afsocket/libafsocket-tls.la \
	modules/afsocket/libafsocket.la
else
modules/afsocket modules/afsocket/ mod-afsocket mod-socket: \
	modules/afsocket/libafsocket-notls.la \
	modules/afsocket/libafsocket.la
endif

BUILT_SOURCES					+=	\
	modules/afsocket/afsocket-grammar.y		\
	modules/afsocket/afsocket-grammar.c		\
	modules/afsocket/afsocket-grammar.h
EXTRA_DIST 					+=	\
	modules/afsocket/afsocket-grammar.ym

if ENABLE_SSL
afsocket-install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	ln -sf libafsocket-tls.so $(DESTDIR)$(moduledir)/libafsocket.so

modules/afsocket/libafsocket.la: modules/afsocket/libafsocket-tls.la
	ln -sf libafsocket-tls.la modules/afsocket/libafsocket.la

else
afsocket-install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(moduledir)
	ln -sf libafsocket-notls.so $(DESTDIR)$(moduledir)/libafsocket.so


modules/afsocket/libafsocket.la: modules/afsocket/libafsocket-tls.la
	ln -sf libafsocket-notls.la modules/afsocket/libafsocket.la

endif

INSTALL_EXEC_HOOKS				+= \
	afsocket-install-exec-hook

afsocket-uninstall-hook:
	rm -f $(DESTDIR)$(moduledir)/libafsocket.so

UNINSTALL_HOOKS					+= \
	afsocket-uninstall-hook

CLEANFILES					+= \
	modules/afsocket/libafsocket.la

.PHONY: modules/afsocket/ mod-afsocket mod-socket

include modules/afsocket/tests/Makefile.am
