if ENABLE_RIEMANN

module_LTLIBRARIES				+= modules/riemann/libriemann.la

modules_riemann_libriemann_la_CFLAGS		 = \
    $(AM_CFLAGS) \
	$(RIEMANN_CLIENT_CFLAGS)		   \
	-I$(top_srcdir)/modules/riemann		   \
	-I$(top_builddir)/modules/riemann

modules_riemann_libriemann_la_SOURCES		 = \
	modules/riemann/riemann-grammar.y	   \
	modules/riemann/riemann.c		   \
	modules/riemann/riemann.h		   \
	modules/riemann/riemann-parser.c	   \
	modules/riemann/riemann-parser.h	   \
	modules/riemann/riemann-plugin.c

modules_riemann_libriemann_la_LIBADD		 = \
	$(MODULE_DEPS_LIBS) $(RIEMANN_CLIENT_LIBS)

modules_riemann_libriemann_la_LDFLAGS		 = \
	$(MODULE_LDFLAGS)
modules_riemann_libriemann_la_DEPENDENCIES	 = \
	$(MODULE_DEPS_LIBS)

modules/riemann mod-riemann: modules/riemann/libriemann.la
else
modules/riemann mod-riemann:
endif

BUILT_SOURCES					+= \
	modules/riemann/riemann-grammar.y	   \
	modules/riemann/riemann-grammar.c	   \
	modules/riemann/riemann-grammar.h

EXTRA_DIST					+= \
	modules/riemann/riemann-grammar.ym	   \
	modules/riemann/riemann-example.conf

.PHONY: modules/riemann mod-riemann
