modules_afsocket_libafsocket_la_SOURCES	=	\
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
	modules/afsocket/afsocket-plugin.c    \
	modules/afsocket/systemd-syslog-source.h       \
	modules/afsocket/systemd-syslog-source.c	\
	modules/afsocket/afsocket-systemd-override.h

module_LTLIBRARIES				+= modules/afsocket/libafsocket.la
modules_afsocket_libafsocket_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) $(libsystemd_CFLAGS)	\
	-I${top_srcdir}/modules/afsocket		\
	-I${top_builddir}/modules/afsocket

modules_afsocket_libafsocket_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(OPENSSL_LIBS)	\
	$(ZLIB_LIBS) $(LIBNET_LIBS)	\
	$(LIBWRAP_LIBS) $(libsystemd_LIBS)
modules_afsocket_libafsocket_la_DEPENDENCIES= 	\
	$(MODULE_DEPS_LIBS)
modules_afsocket_libafsocket_la_LDFLAGS	= 	\
	$(MODULE_LDFLAGS)

modules/afsocket modules/afsocket/ mod-afsocket mod-socket: \
	modules/afsocket/libafsocket.la

BUILT_SOURCES					+=	\
	modules/afsocket/afsocket-grammar.y		\
	modules/afsocket/afsocket-grammar.c		\
	modules/afsocket/afsocket-grammar.h
EXTRA_DIST 					+=	\
	modules/afsocket/afsocket-grammar.ym	\
	modules/afsocket/CMakeLists.txt

CLEANFILES					+= \
	modules/afsocket/libafsocket.la

.PHONY: modules/afsocket/ mod-afsocket mod-socket

include modules/afsocket/tests/Makefile.am
