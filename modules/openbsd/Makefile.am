if ENABLE_OPENBSD_SYSTEM_SOURCE

module_LTLIBRARIES      += modules/openbsd/libopenbsd.la
modules_openbsd_libopenbsd_la_SOURCES = \
  modules/openbsd/openbsd-grammar.y       \
  modules/openbsd/openbsd-parser.c        \
  modules/openbsd/openbsd-parser.h        \
  modules/openbsd/openbsd-plugin.c        \
  modules/openbsd/openbsd-driver.c 		\
  modules/openbsd/openbsd-driver.h 		

BUILT_SOURCES       +=      \
  modules/openbsd/openbsd-grammar.y       \
  modules/openbsd/openbsd-grammar.c       \
  modules/openbsd/openbsd-grammar.h

EXTRA_DIST        +=      \
  modules/openbsd/openbsd-grammar.ym

modules_openbsd_libopenbsd_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/openbsd        \
  -I$(top_builddir)/modules/openbsd
modules_openbsd_libopenbsd_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_openbsd_libopenbsd_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_openbsd_libopenbsd_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/openbsd modules/openbsd/ mod-openbsd: modules/openbsd/libopenbsd.la
endif
.PHONY: modules/openbsd/ mod-openbsd
