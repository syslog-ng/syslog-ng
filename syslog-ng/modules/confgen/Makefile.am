module_LTLIBRARIES				+= modules/confgen/libconfgen.la
modules_confgen_libconfgen_la_SOURCES		=	\
	modules/confgen/confgen.h			\
	modules/confgen/confgen-plugin.c
modules_confgen_libconfgen_la_CPPFLAGS		=	\
	$(AM_CPPFLAGS)
modules_confgen_libconfgen_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS)
modules_confgen_libconfgen_la_LDFLAGS		=	\
	$(MODULE_LDFLAGS)
modules_confgen_libconfgen_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

EXTRA_DIST += modules/confgen/CMakeLists.txt

modules/confgen modules/confgen/ mod-confgen: modules/confgen/libconfgen.la
.PHONY: modules/confgen/ mod-confgen

include modules/confgen/tests/Makefile.am
