if ENABLE_DARWIN_OSL

AM_LIBTOOLFLAGS="--tag=CC"

module_LTLIBRARIES += modules/darwinosl/libdarwinosl.la
modules_darwinosl_libdarwinosl_la_SOURCES = \
modules/darwinosl/darwinosl-plugin.c \
modules/darwinosl/darwinosl-source.m \
modules/darwinosl/darwinosl-source.h \
modules/darwinosl/darwinosl-source-oslog.m \
modules/darwinosl/darwinosl-source-oslog.h \
modules/darwinosl/darwinosl-source-persist.c \
modules/darwinosl/darwinosl-source-persist.h \
modules/darwinosl/darwinosl-parser.c \
modules/darwinosl/darwinosl-parser.h \
modules/darwinosl/darwinosl-grammar.y

modules_darwinosl_libdarwinosl_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/darwinosl -I$(top_builddir)/modules/darwinosl
modules_darwinosl_libdarwinosl_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_darwinosl_libdarwinosl_la_LDFLAGS = $(MODULE_LDFLAGS) -ObjC -framework Foundation -framework OSLog
EXTRA_modules_darwinosl_libdarwinosl_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

modules/darwinosl mod-darwinosl: modules/darwinosl/libdarwinosl.la
else
modules/darwinosl mod-darwinosl:
endif

BUILT_SOURCES += \
	modules/darwinosl/darwinosl-grammar.y \
	modules/darwinosl/darwinosl-grammar.c \
	modules/darwinosl/darwinosl-grammar.h

EXTRA_DIST +=	\
	modules/darwinosl/darwinosl-grammar.ym \
	modules/darwinosl/CMakeLists.txt

.PHONY: modules/darwinosl mod-darwinosl

#include modules/darwinosl/tests/Makefile.am
