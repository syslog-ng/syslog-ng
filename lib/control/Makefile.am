controlincludedir			= ${pkgincludedir}/control

controlinclude_HEADERS = \
	lib/control/control.h			\
	lib/control/control-commands.h		\
	lib/control/control-main.h		\
	lib/control/control-server.h

control_sources = \
	lib/control/control.c			\
	lib/control/control-commands.c		\
	lib/control/control-main.c		\
	lib/control/control-server.c		\
	lib/control/control-server-unix.c

EXTRA_DIST += lib/control/control-server-unix.c

include lib/control/tests/Makefile.am
