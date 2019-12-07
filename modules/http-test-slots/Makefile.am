module_LTLIBRARIES += modules/http-test-slots/libhttp-test-slots.la

modules_http_test_slots_libhttp_test_slots_la_SOURCES = \
  modules/http-test-slots/http-test-slots.c \
  modules/http-test-slots/http-test-slots.h \
  modules/http-test-slots/http-test-slots-grammar.y \
  modules/http-test-slots/http-test-slots-parser.c \
  modules/http-test-slots/http-test-slots-parser.h \
  modules/http-test-slots/http-test-slots-plugin.c

BUILT_SOURCES += \
  modules/http-test-slots/http-test-slots-grammar.y \
  modules/http-test-slots/http-test-slots-grammar.c \
  modules/http-test-slots/http-test-slots-grammar.h

EXTRA_DIST += modules/http-test-slots/http-test-slots-grammar.ym

modules_http_test_slots_libhttp_test_slots_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/http-test-slots -I$(top_builddir)/modules/http-test-slots
modules_http_test_slots_libhttp_test_slots_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_http_test_slots_libhttp_test_slots_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_http_test_slots_libhttp_test_slots_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

modules/http-test-slots modules/http-test-slots/ mod-http-test-slots: modules/http-test-slots/libhttp-test-slots.la

.PHONY: modules/http-test-slots/ mod-http-test-slots
