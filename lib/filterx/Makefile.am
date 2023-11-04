filterxincludedir			= ${pkgincludedir}/filterx

filterxinclude_HEADERS = 			\
	lib/filterx/filterx-parser.h		\
	lib/filterx/filterx-globals.h		\
	lib/filterx/filterx-object.h		\
	lib/filterx/filterx-parser.h


filterx_sources = 				\
	lib/filterx/filterx-parser.c		\
	lib/filterx/filterx-globals.c		\
	lib/filterx/filterx-object.c		\
	lib/filterx/filterx-parser.c		\
	lib/filterx/filterx-grammar.y

BUILT_SOURCES += 				\
	lib/filterx/filterx-grammar.y	\
	lib/filterx/filterx-grammar.c	\
	lib/filterx/filterx-grammar.h

EXTRA_DIST += lib/filterx/filterx-grammar.ym \
	lib/filterx/CMakeLists.txt

include lib/filterx/tests/Makefile.am
