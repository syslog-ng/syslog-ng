
appmodel_sources =

module_LTLIBRARIES				+= modules/appmodel/libappmodel.la
modules_appmodel_libappmodel_la_SOURCES	=	\
	modules/appmodel/appmodel.c		\
	modules/appmodel/appmodel.h		\
	modules/appmodel/appmodel-parser.c	\
	modules/appmodel/appmodel-parser.h	\
	modules/appmodel/appmodel-plugin.c	\
	modules/appmodel/appmodel-context.c	\
	modules/appmodel/appmodel-context.h	\
	modules/appmodel/app-parser-generator.c \
	modules/appmodel/app-parser-generator.h \
	modules/appmodel/application.c		\
	modules/appmodel/application.h		\
	modules/appmodel/appmodel-grammar.y

modules_appmodel_libappmodel_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/appmodel		\
	-I$(top_builddir)/modules/appmodel
modules_appmodel_libappmodel_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_appmodel_libappmodel_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_appmodel_libappmodel_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=	\
	modules/appmodel/appmodel-grammar.y		\
	modules/appmodel/appmodel-grammar.c		\
	modules/appmodel/appmodel-grammar.h
EXTRA_DIST					+=	\
	modules/appmodel/appmodel-grammar.ym	\
	modules/appmodel/CMakeLists.txt

modules/appmodel modules/appmodel/ mod-tags-parser: modules/appmodel/libappmodel.la
.PHONY: modules/appmodel/ mod-tags-parser



include modules/appmodel/tests/Makefile.am
