templateincludedir			= ${pkgincludedir}/template

EXTRA_DIST += lib/template/CMakeLists.txt	\
	lib/template/tests/CMakeLists.txt

templateinclude_HEADERS = \
	lib/template/templates.h		\
	lib/template/macros.h			\
	lib/template/function.h			\
	lib/template/simple-function.h		\
	lib/template/repr.h			\
	lib/template/compiler.h			\
	lib/template/user-function.h		\
	lib/template/escaping.h			\
	lib/template/common-template-typedefs.h

template_sources = \
	lib/template/templates.c		\
	lib/template/macros.c			\
	lib/template/simple-function.c		\
	lib/template/repr.c			\
	lib/template/compiler.c			\
	lib/template/user-function.c		\
	lib/template/escaping.c

include lib/template/tests/Makefile.am
