controlincludedir			= ${pkgincludedir}/control

EXTRA_DIST +=	\
	lib/control/CMakeLists.txt

controlinclude_HEADERS = \
	lib/control/control.h			\
	lib/control/control-commands.h		\
	lib/control/control-command-thread.h	\
	lib/control/control-main.h		\
	lib/control/control-server.h		\
	lib/control/control-server-unix.h	\
	lib/control/control-connection.h

control_sources = \
	lib/control/control-commands.c		\
	lib/control/control-command-thread.c	\
	lib/control/control-main.c		\
	lib/control/control-server.c		\
	lib/control/control-server-unix.c	\
	lib/control/control-connection.c

EXTRA_DIST += lib/control/control-server-unix.c

include lib/control/tests/Makefile.am
