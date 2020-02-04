rewriteincludedir			= ${pkgincludedir}/rewrite

EXTRA_DIST += lib/rewrite/CMakeLists.txt

rewriteinclude_HEADERS = 			\
	lib/rewrite/rewrite-expr.h		\
	lib/rewrite/rewrite-set-tag.h		\
	lib/rewrite/rewrite-set.h		\
	lib/rewrite/rewrite-unset.h		\
	lib/rewrite/rewrite-subst.h		\
	lib/rewrite/rewrite-expr-parser.h	\
	lib/rewrite/rewrite-groupset.h		\
	lib/rewrite/rewrite-set-severity.h


rewrite_sources = 				\
	lib/rewrite/rewrite-expr.c		\
	lib/rewrite/rewrite-set-tag.c		\
	lib/rewrite/rewrite-set.c		\
	lib/rewrite/rewrite-unset.c		\
	lib/rewrite/rewrite-subst.c		\
	lib/rewrite/rewrite-expr-parser.c	\
	lib/rewrite/rewrite-expr-grammar.y	\
	lib/rewrite/rewrite-groupset.c		\
	lib/rewrite/rewrite-set-severity.c

BUILT_SOURCES += 				\
	lib/rewrite/rewrite-expr-grammar.y	\
	lib/rewrite/rewrite-expr-grammar.c	\
	lib/rewrite/rewrite-expr-grammar.h

EXTRA_DIST += lib/rewrite/rewrite-expr-grammar.ym

include lib/rewrite/tests/Makefile.am
