if ENABLE_NATIVE

export top_srcdir
export top_builddir

lib_LIBRARIES = modules/native/libsyslog-ng-native-connector.a
modules_native_libsyslog_ng_native_connector_a_SOURCES = \
  modules/native/native-grammar.y       \
  modules/native/native-parser.c        \
  modules/native/native-parser.h        \
  modules/native/parser.c				\
  modules/native/parser.h

pkgconfig_DATA += syslog-ng-native-connector.pc

modules_native_libsyslog_ng_native_connector_a_CPPFLAGS = \
  $(AM_CPPFLAGS)	-fPIC	\
  -fvisibility=hidden	\
  -I$(top_srcdir)/modules/native        \
  -I$(top_builddir)/modules/native

else
modules/native modules/native/ mod-native: modules/native/libnative.la
endif

BUILT_SOURCES       +=      \
  modules/native/native-grammar.y       \
  modules/native/native-grammar.c       \
  modules/native/native-grammar.h

EXTRA_DIST        +=      \
  modules/native/native-grammar.ym	\
  modules/native/CMakeLists.txt

.PHONY: modules/native/ mod-native
