module_LTLIBRARIES			+= modules/afuser/libafuser.la
modules_afuser_libafuser_la_SOURCES	=	\
	modules/afuser/afuser.c			\
	modules/afuser/afuser.h			\
	modules/afuser/afuser-grammar.y		\
	modules/afuser/afuser-parser.c		\
	modules/afuser/afuser-parser.h		\
	modules/afuser/afuser-plugin.c

modules_afuser_libafuser_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/afuser		\
	-I$(top_builddir)/modules/afuser
modules_afuser_libafuser_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_afuser_libafuser_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afuser_libafuser_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES				+=	\
	modules/afuser/afuser-grammar.y		\
	modules/afuser/afuser-grammar.c		\
	modules/afuser/afuser-grammar.h
EXTRA_DIST				+=	\
	modules/afuser/afuser-grammar.ym	\
	modules/afuser/CMakeLists.txt

modules/afuser modules/afuser/ mod-afuser mod-usertty: \
	modules/afuser/libafuser.la
.PHONY: modules/afuser/ mod-afuser mod-usertty
