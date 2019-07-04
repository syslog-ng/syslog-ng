filterincludedir			= ${pkgincludedir}/filter

filterinclude_HEADERS = 			\
	lib/filter/filter-expr.h		\
	lib/filter/filter-op.h			\
	lib/filter/filter-cmp.h			\
	lib/filter/filter-in-list.h		\
	lib/filter/filter-tags.h		\
	lib/filter/filter-netmask.h		\
	lib/filter/filter-netmask6.h	\
	lib/filter/filter-call.h		\
	lib/filter/filter-re.h			\
	lib/filter/filter-pri.h			\
	lib/filter/filter-pipe.h		\
	lib/filter/filter-expr-parser.h

filter_sources = 				\
	lib/filter/filter-expr.c		\
	lib/filter/filter-op.c			\
	lib/filter/filter-cmp.c			\
	lib/filter/filter-in-list.c		\
	lib/filter/filter-tags.c		\
	lib/filter/filter-netmask.c		\
	lib/filter/filter-netmask6.c	\
	lib/filter/filter-call.c		\
	lib/filter/filter-re.c			\
	lib/filter/filter-pri.c			\
	lib/filter/filter-pipe.c		\
	lib/filter/filter-expr-parser.c		\
	lib/filter/filter-expr-grammar.y

BUILT_SOURCES += 				\
	lib/filter/filter-expr-grammar.y	\
	lib/filter/filter-expr-grammar.c	\
	lib/filter/filter-expr-grammar.h

EXTRA_DIST += lib/filter/filter-expr-grammar.ym \
	lib/filter/CMakeLists.txt

include lib/filter/tests/Makefile.am
include lib/filter/optimizer/Makefile.am
