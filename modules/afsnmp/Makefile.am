if ENABLE_AFSNMP

module_LTLIBRARIES				+= modules/afsnmp/libafsnmp.la
modules_afsnmp_libafsnmp_la_SOURCES	=	\
	modules/afsnmp/afsnmp-grammar.y \
	modules/afsnmp/afsnmpdest.c \
	modules/afsnmp/afsnmp-plugin.c \
	modules/afsnmp/afsnmpdest.h \
	modules/afsnmp/afsnmp-parser.c \
	modules/afsnmp/afsnmp-parser.h \
	modules/afsnmp/snmptrapd-header-parser.h \
	modules/afsnmp/snmptrapd-header-parser.c \
	modules/afsnmp/snmptrapd-nv-context.h \
	modules/afsnmp/snmptrapd-parser.h \
	modules/afsnmp/snmptrapd-parser.c \
	modules/afsnmp/varbindlist-scanner.h \
	modules/afsnmp/varbindlist-scanner.c


modules_afsnmp_libafsnmp_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	$(NETSNMP_CFLAGS)	\
	-I$(top_srcdir)/modules/afsnmp		\
	-I$(top_builddir)/modules/afsnmp

modules_afsnmp_libafsnmp_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)

modules_afsnmp_libafsnmp_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS) \
	$(NETSNMP_LIBS)

modules_afsnmp_libafsnmp_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/afsnmp mod-afsnmp: modules/afsnmp/libafsnmp.la
else
modules/afsnmp mod-afsnmp:
endif

BUILT_SOURCES					+=	\
	modules/afsnmp/afsnmp-grammar.y \
	modules/afsnmp/afsnmp-grammar.c \
	modules/afsnmp/afsnmp-grammar.h

EXTRA_DIST					+=	\
	modules/afsnmp/afsnmp-grammar.ym	\
	modules/afsnmp/CMakeLists.txt

.PHONY: modules/afsnmp mod-afsnmp

include modules/afsnmp/tests/Makefile.am
