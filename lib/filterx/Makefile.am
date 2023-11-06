filterxincludedir			= ${pkgincludedir}/filterx

filterxinclude_HEADERS = 			\
	lib/filterx/filterx-parser.h		\
	lib/filterx/filterx-globals.h		\
	lib/filterx/filterx-expr.h		\
	lib/filterx/expr-literal.h		\
	lib/filterx/expr-template.h		\
	lib/filterx/expr-boolalg.h		\
	lib/filterx/expr-getattr.h		\
	lib/filterx/expr-setattr.h		\
	lib/filterx/expr-json.h			\
	lib/filterx/expr-message-ref.h		\
	lib/filterx/filterx-object.h		\
	lib/filterx/object-primitive.h		\
	lib/filterx/filterx-scope.h		\
	lib/filterx/filterx-eval.h		\
	lib/filterx/object-json.h		\
	lib/filterx/object-string.h		\
	lib/filterx/object-datetime.h		\
	lib/filterx/object-list.h		\
	lib/filterx/object-null.h		\
	lib/filterx/object-message-value.h	\
	lib/filterx/object-message-ref.h	\
	lib/filterx/filterx-conversions.h	\
	lib/filterx/filterx-parser.h		\
	lib/filterx/filterx-pipe.h


filterx_sources = 				\
	lib/filterx/filterx-parser.c		\
	lib/filterx/filterx-globals.c		\
	lib/filterx/filterx-expr.c		\
	lib/filterx/expr-literal.c		\
	lib/filterx/expr-template.c		\
	lib/filterx/expr-boolalg.c		\
	lib/filterx/expr-getattr.c		\
	lib/filterx/expr-setattr.c		\
	lib/filterx/expr-json.c			\
	lib/filterx/expr-message-ref.c		\
	lib/filterx/filterx-object.c		\
	lib/filterx/object-primitive.c		\
	lib/filterx/filterx-scope.c		\
	lib/filterx/filterx-eval.c		\
	lib/filterx/object-json.c		\
	lib/filterx/object-string.c		\
	lib/filterx/object-datetime.c		\
	lib/filterx/object-list.c		\
	lib/filterx/object-null.c		\
	lib/filterx/object-message-value.c	\
	lib/filterx/object-message-ref.c	\
	lib/filterx/filterx-conversions.c	\
	lib/filterx/filterx-parser.c		\
	lib/filterx/filterx-pipe.c		\
	lib/filterx/filterx-grammar.y

BUILT_SOURCES += 				\
	lib/filterx/filterx-grammar.y	\
	lib/filterx/filterx-grammar.c	\
	lib/filterx/filterx-grammar.h

EXTRA_DIST += lib/filterx/filterx-grammar.ym \
	lib/filterx/CMakeLists.txt

include lib/filterx/tests/Makefile.am
