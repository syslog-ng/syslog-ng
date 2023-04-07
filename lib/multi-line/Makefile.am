multilineincludedir			= ${pkgincludedir}/multi-line


multilineinclude_HEADERS = \
	lib/multi-line/multi-line-logic.h \
	lib/multi-line/multi-line-factory.h \
	lib/multi-line/indented-multi-line.h \
	lib/multi-line/regexp-multi-line.h \
	lib/multi-line/multi-line-pattern.h \
	lib/multi-line/smart-multi-line.h


multiline_sources = \
	lib/multi-line/multi-line-logic.c \
	lib/multi-line/multi-line-factory.c \
	lib/multi-line/indented-multi-line.c \
	lib/multi-line/regexp-multi-line.c \
	lib/multi-line/multi-line-pattern.c \
	lib/multi-line/smart-multi-line.c

pkgdata_DATA = lib/multi-line/smart-multi-line.fsm

EXTRA_DIST += lib/multi-line/CMakeLists.txt \
	lib/multi-line/smart-multi-line.fsm

include lib/multi-line/tests/Makefile.am
