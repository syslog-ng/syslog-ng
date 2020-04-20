debuggerincludedir			= ${pkgincludedir}/debugger

EXTRA_DIST += lib/debugger/CMakeLists.txt

debuggerinclude_HEADERS = \
	lib/debugger/debugger.h		\
	lib/debugger/tracer.h		\
	lib/debugger/debugger-main.h

debugger_sources = \
	lib/debugger/debugger.c		\
	lib/debugger/tracer.c		\
	lib/debugger/debugger-main.c

include lib/debugger/tests/Makefile.am
