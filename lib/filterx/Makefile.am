filterxincludedir			= ${pkgincludedir}/filterx

filterxinclude_HEADERS = 			\
	lib/filterx/filterx-parser.h		\
	lib/filterx/filterx-globals.h		\
	lib/filterx/filterx-expr.h		\
	lib/filterx/expr-literal.h		\
	lib/filterx/expr-template.h		\
	lib/filterx/expr-boolalg.h		\
	lib/filterx/expr-assign.h		\
	lib/filterx/expr-getattr.h		\
	lib/filterx/expr-setattr.h		\
	lib/filterx/expr-get-subscript.h	\
	lib/filterx/expr-set-subscript.h	\
	lib/filterx/expr-variable.h		\
	lib/filterx/expr-comparison.h		\
	lib/filterx/filterx-object.h		\
	lib/filterx/filterx-weakrefs.h		\
	lib/filterx/object-primitive.h		\
	lib/filterx/filterx-scope.h		\
	lib/filterx/filterx-eval.h		\
	lib/filterx/object-json.h		\
	lib/filterx/object-json-internal.h		\
	lib/filterx/object-string.h		\
	lib/filterx/object-datetime.h		\
	lib/filterx/object-null.h		\
	lib/filterx/object-message-value.h	\
	lib/filterx/object-list-interface.h	\
	lib/filterx/object-dict-interface.h	\
	lib/filterx/filterx-config.h		\
	lib/filterx/filterx-pipe.h		\
	lib/filterx/expr-function.h	\
	lib/filterx/expr-condition.h \
	lib/filterx/expr-isset.h \
	lib/filterx/expr-unset.h		\
	lib/filterx/expr-shorthand.h		\
	lib/filterx/expr-generator.h		\
	lib/filterx/expr-literal-generator.h		\
	lib/filterx/expr-regexp.h		\
	lib/filterx/func-istype.h		\
	lib/filterx/func-len.h		\
	lib/filterx/filterx-private.h


filterx_sources = 				\
	lib/filterx/filterx-parser.c		\
	lib/filterx/filterx-globals.c		\
	lib/filterx/filterx-expr.c		\
	lib/filterx/expr-literal.c		\
	lib/filterx/expr-template.c		\
	lib/filterx/expr-boolalg.c		\
	lib/filterx/expr-assign.c		\
	lib/filterx/expr-getattr.c		\
	lib/filterx/expr-setattr.c		\
	lib/filterx/expr-get-subscript.c	\
	lib/filterx/expr-set-subscript.c	\
	lib/filterx/expr-variable.c		\
	lib/filterx/expr-comparison.c		\
	lib/filterx/filterx-object.c		\
	lib/filterx/filterx-weakrefs.c		\
	lib/filterx/object-primitive.c		\
	lib/filterx/filterx-scope.c		\
	lib/filterx/filterx-eval.c		\
	lib/filterx/object-json.c		\
	lib/filterx/object-json-object.c		\
	lib/filterx/object-json-array.c		\
	lib/filterx/object-string.c		\
	lib/filterx/object-datetime.c		\
	lib/filterx/object-null.c		\
	lib/filterx/object-message-value.c	\
	lib/filterx/object-list-interface.c	\
	lib/filterx/object-dict-interface.c	\
	lib/filterx/filterx-config.c		\
	lib/filterx/filterx-pipe.c		\
	lib/filterx/expr-function.c		\
	lib/filterx/expr-condition.c		\
	lib/filterx/expr-isset.c		\
	lib/filterx/expr-unset.c		\
	lib/filterx/expr-shorthand.c		\
	lib/filterx/expr-generator.c		\
	lib/filterx/expr-literal-generator.c		\
	lib/filterx/expr-regexp.c		\
	lib/filterx/func-istype.c		\
	lib/filterx/func-len.c		\
	lib/filterx/filterx-private.c	\
	lib/filterx/filterx-grammar.y

BUILT_SOURCES += 				\
	lib/filterx/filterx-grammar.y	\
	lib/filterx/filterx-grammar.c	\
	lib/filterx/filterx-grammar.h

EXTRA_DIST += lib/filterx/filterx-grammar.ym \
	lib/filterx/CMakeLists.txt

include lib/filterx/tests/Makefile.am
