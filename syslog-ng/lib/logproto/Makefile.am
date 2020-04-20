logprotoincludedir			= ${pkgincludedir}/logproto

EXTRA_DIST += lib/logproto/CMakeLists.txt

logprotoinclude_HEADERS = \
	lib/logproto/logproto-client.h	\
	lib/logproto/logproto-server.h	\
	lib/logproto/logproto-buffered-server.h \
	lib/logproto/logproto-dgram-server.h	\
	lib/logproto/logproto-framed-client.h	\
	lib/logproto/logproto-framed-server.h	\
	lib/logproto/logproto-text-client.h  \
	lib/logproto/logproto-text-server.h	\
	lib/logproto/logproto-indented-multiline-server.h \
	lib/logproto/logproto-regexp-multiline-server.h \
	lib/logproto/logproto-multiline-server.h \
	lib/logproto/logproto-record-server.h \
	lib/logproto/logproto-builtins.h	\
	lib/logproto/logproto.h

logproto_sources = \
	lib/logproto/logproto-client.c	\
	lib/logproto/logproto-server.c	\
	lib/logproto/logproto-buffered-server.c \
	lib/logproto/logproto-dgram-server.c	\
	lib/logproto/logproto-framed-client.c	\
	lib/logproto/logproto-framed-server.c	\
	lib/logproto/logproto-text-client.c  \
	lib/logproto/logproto-text-server.c	\
	lib/logproto/logproto-indented-multiline-server.c \
	lib/logproto/logproto-regexp-multiline-server.c \
	lib/logproto/logproto-multiline-server.c \
	lib/logproto/logproto-record-server.c \
	lib/logproto/logproto-builtins.c

include lib/logproto/tests/Makefile.am
