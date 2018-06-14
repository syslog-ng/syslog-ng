module_LTLIBRARIES      += modules/snmptrapd-parser/libsnmptrapd-parser.la
modules_snmptrapd_parser_libsnmptrapd_parser_la_SOURCES =   \
  modules/snmptrapd-parser/snmptrapd-parser.c               \
  modules/snmptrapd-parser/snmptrapd-parser.h               \
  modules/snmptrapd-parser/snmptrapd-parser-grammar.y       \
  modules/snmptrapd-parser/snmptrapd-parser-parser.c        \
  modules/snmptrapd-parser/snmptrapd-parser-parser.h        \
  modules/snmptrapd-parser/snmptrapd-parser-plugin.c        \
  modules/snmptrapd-parser/varbindlist-scanner.h            \
  modules/snmptrapd-parser/varbindlist-scanner.c            \
  modules/snmptrapd-parser/snmptrapd-header-parser.h        \
  modules/snmptrapd-parser/snmptrapd-header-parser.c        \
  modules/snmptrapd-parser/snmptrapd-nv-context.h

BUILT_SOURCES       += \
  modules/snmptrapd-parser/snmptrapd-parser-grammar.y       \
  modules/snmptrapd-parser/snmptrapd-parser-grammar.c       \
  modules/snmptrapd-parser/snmptrapd-parser-grammar.h

EXTRA_DIST          += \
  modules/snmptrapd-parser/snmptrapd-parser-grammar.ym	\
  modules/snmptrapd-parser/CMakeLists.txt

modules_snmptrapd_parser_libsnmptrapd_parser_la_CPPFLAGS =  \
  $(AM_CPPFLAGS)                                            \
  -I$(top_srcdir)/modules/snmptrapd-parser                  \
  -I$(top_builddir)/modules/snmptrapd-parser
modules_snmptrapd_parser_libsnmptrapd_parser_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_snmptrapd_parser_libsnmptrapd_parser_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_snmptrapd_parser_libsnmptrapd_parser_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/snmptrapd-parser modules/snmptrapd-parser/ mod-snmptrapd-parser: modules/snmptrapd-parser/libsnmptrapd-parser.la
.PHONY: modules/snmptrapd-parser/ mod-snmptrapd-parser

include modules/snmptrapd-parser/tests/Makefile.am
