module_LTLIBRARIES			+= modules/afprog/libafprog.la
modules_afprog_libafprog_la_SOURCES	= 	\
	modules/afprog/afprog.c			\
	modules/afprog/afprog.h			\
	modules/afprog/afprog-grammar.y		\
	modules/afprog/afprog-parser.c		\
	modules/afprog/afprog-parser.h		\
	modules/afprog/afprog-plugin.c

BUILT_SOURCES				+=	\
	modules/afprog/afprog-grammar.y		\
	modules/afprog/afprog-grammar.c		\
	modules/afprog/afprog-grammar.h
EXTRA_DIST				+=	\
	modules/afprog/afprog-grammar.ym	\
	modules/afprog/CMakeLists.txt

modules_afprog_libafprog_la_CPPFLAGS	= 	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/afprog		\
	-I$(top_builddir)/modules/afprog
modules_afprog_libafprog_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_afprog_libafprog_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afprog_libafprog_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

modules/afprog modules/afprog/ mod-afprog mod-prog: \
	modules/afprog/libafprog.la
.PHONY: modules/afprog/ mod-afprog mod-prog
