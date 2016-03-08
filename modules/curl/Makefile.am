if ENABLE_CURL

module_LTLIBRARIES      += modules/curl/libcurl.la
modules_curl_libcurl_la_SOURCES = \
  modules/curl/curl-plugin.h        \
  modules/curl/curl.c               \
  modules/curl/curl-grammar.y       \
  modules/curl/curl-parser.c        \
  modules/curl/curl-parser.h        \
  modules/curl/curl-plugin.c

BUILT_SOURCES       +=      \
  modules/curl/curl-grammar.y       \
  modules/curl/curl-grammar.c       \
  modules/curl/curl-grammar.h

modules_curl_libcurl_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  $(LIBCURL_CFLAGS)          \
  -I$(top_srcdir)/modules/curl        \
  -I$(top_builddir)/modules/curl

modules_curl_libcurl_la_LIBADD  = $(MODULE_DEPS_LIBS) $(LIBCURL_LIBS)

modules_curl_libcurl_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_curl_libcurl_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/curl modules/curl/ mod-curl: modules/curl/libcurl.la
else
modules/curl modules/curl/ mod-curl:
endif

EXTRA_DIST        +=      \
  modules/curl/curl-grammar.ym


.PHONY: modules/curl/ mod-curl
