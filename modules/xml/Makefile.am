module_LTLIBRARIES      += modules/xml/libxml.la
modules_xml_libxml_la_SOURCES = \
  modules/xml/xml-grammar.y       \
  modules/xml/xml-parser.c        \
  modules/xml/xml-parser.h        \
  modules/xml/xml-plugin.c        \
  modules/xml/xml-scanner.c       \
  modules/xml/xml-scanner.h       \
  modules/xml/xml.h               \
  modules/xml/xml.c


BUILT_SOURCES       +=      \
  modules/xml/xml-grammar.y       \
  modules/xml/xml-grammar.c       \
  modules/xml/xml-grammar.h

EXTRA_DIST        +=      \
  modules/xml/xml-grammar.ym	\
  modules/xml/CMakeLists.txt

modules_xml_libxml_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/xml        \
  -I$(top_builddir)/modules/xml
modules_xml_libxml_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_xml_libxml_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_xml_libxml_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/xml modules/xml/ mod-xml: modules/xml/libxml.la
.PHONY: modules/xml/ mod-xml

include modules/xml/tests/Makefile.am
