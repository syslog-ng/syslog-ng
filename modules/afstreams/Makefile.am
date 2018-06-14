if ENABLE_SUN_STREAMS

module_LTLIBRARIES				+= modules/afstreams/libafstreams.la
modules_afstreams_libafstreams_la_SOURCES	=	\
	modules/afstreams/afstreams.c 			\
	modules/afstreams/afstreams.h			\
	modules/afstreams/afstreams-grammar.y		\
	modules/afstreams/afstreams-parser.c		\
	modules/afstreams/afstreams-parser.h		\
	modules/afstreams/afstreams-plugin.c

modules_afstreams_libafstreams_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/afstreams		\
	-I$(top_builddir)/modules/afstreams
modules_afstreams_libafstreams_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_afstreams_libafstreams_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afstreams_libafstreams_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/afstreams modules/afstreams/ mod-afstreams mod-streams: \
	modules/afstreams/libafstreams.la
else
modules/afstreams modules/afstreams/ mod-afstreams mod-streams:
endif

BUILT_SOURCES					+=	\
	modules/afstreams/afstreams-grammar.y		\
	modules/afstreams/afstreams-grammar.c		\
	modules/afstreams/afstreams-grammar.h
EXTRA_DIST					+=	\
	modules/afstreams/afstreams-grammar.ym	\
	modules/afstreams/CMakeLists.txt

.PHONY: modules/afstreams/ mod-afstreams mod-streams
