transportincludedir			= ${pkgincludedir}/transport

transportinclude_HEADERS = \
	lib/transport/logtransport.h	\
	lib/transport/transport-aux-data.h	\
	lib/transport/transport-tls.h	\
	lib/transport/transport-file.h	\
	lib/transport/transport-pipe.h	\
	lib/transport/transport-device.h \
	lib/transport/transport-socket.h

transport_sources = \
	lib/transport/logtransport.c	\
	lib/transport/transport-aux-data.c	\
	lib/transport/transport-file.c	\
	lib/transport/transport-pipe.c	\
	lib/transport/transport-device.c \
	lib/transport/transport-socket.c

transport_crypto_sources = \
	lib/transport/transport-tls.c

include lib/transport/tests/Makefile.am
