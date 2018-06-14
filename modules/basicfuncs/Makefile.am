module_LTLIBRARIES				+= modules/basicfuncs/libbasicfuncs.la

modules_basicfuncs_libbasicfuncs_la_SOURCES	=	\
	modules/basicfuncs/basic-funcs.c

modules_basicfuncs_libbasicfuncs_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/basicfuncs
modules_basicfuncs_libbasicfuncs_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_basicfuncs_libbasicfuncs_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_basicfuncs_libbasicfuncs_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

EXTRA_DIST					+=	\
	modules/basicfuncs/urlencode.c			\
	modules/basicfuncs/cond-funcs.c		\
	modules/basicfuncs/context-funcs.c		\
	modules/basicfuncs/numeric-funcs.c		\
	modules/basicfuncs/str-funcs.c			\
	modules/basicfuncs/fname-funcs.c		\
	modules/basicfuncs/ip-funcs.c			\
	modules/basicfuncs/misc-funcs.c			\
	modules/basicfuncs/list-funcs.c			\
	modules/basicfuncs/tf-template.c		\
	modules/basicfuncs/CMakeLists.txt

modules/basicfuncs modules/basicfuncs/ mod-basicfuncs: modules/basicfuncs/libbasicfuncs.la
.PHONY: modules/basicfuncs/ mod-basicfuncs

include modules/basicfuncs/tests/Makefile.am
