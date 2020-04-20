if ENABLE_STOMP

module_LTLIBRARIES += modules/afstomp/libafstomp.la
modules_afstomp_libafstomp_la_CPPFLAGS = \
    $(AM_CPPFLAGS)  \
    -I$(top_srcdir)/modules/afstomp    \
    -I$(top_builddir)/modules/afstomp

modules_afstomp_libafstomp_la_SOURCES = \
    modules/afstomp/afstomp-grammar.y \
    modules/afstomp/afstomp.c \
    modules/afstomp/afstomp.h \
    modules/afstomp/afstomp-parser.c \
    modules/afstomp/afstomp-parser.h \
    modules/afstomp/stomp.c \
    modules/afstomp/stomp.h
modules_afstomp_libafstomp_la_LIBADD = \
    $(MODULE_DEPS_LIBS)
modules_afstomp_libafstomp_la_LDFLAGS = \
    $(MODULE_LDFLAGS)
modules_afstomp_libafstomp_la_DEPENDENCIES  = \
        $(MODULE_DEPS_LIBS)

modules/afstomp modules/afstomp/ mod-afstomp mod-stomp: \
    modules/afstomp/libafstomp.la

else
modules/afstomp modules/afstomp/ mod-afstomp mod-stomp:
endif

BUILT_SOURCES += \
    modules/afstomp/afstomp-grammar.y \
    modules/afstomp/afstomp-grammar.c \
    modules/afstomp/afstomp-grammar.h

EXTRA_DIST += \
    modules/afstomp/afstomp-grammar.ym	\
    modules/afstomp/CMakeLists.txt

.PHONY: modules/afstomp/ mod-afstomp mod-stomp

include modules/afstomp/tests/Makefile.am
