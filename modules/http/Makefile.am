if ENABLE_HTTP

module_LTLIBRARIES      += modules/http/libcurl.la
modules_http_libcurl_la_SOURCES = \
  modules/http/http-plugin.h        \
  modules/http/http.c               \
  modules/http/http-grammar.y       \
  modules/http/http-parser.c        \
  modules/http/http-parser.h        \
  modules/http/http-plugin.c

BUILT_SOURCES       +=      \
  modules/http/http-grammar.y       \
  modules/http/http-grammar.c       \
  modules/http/http-grammar.h

modules_http_libcurl_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  $(LIBCURL_CFLAGS)          \
  -I$(top_srcdir)/modules/http        \
  -I$(top_builddir)/modules/http

modules_http_libcurl_la_LIBADD  = $(MODULE_DEPS_LIBS) $(LIBCURL_LIBS)

modules_http_libcurl_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_http_libcurl_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/http modules/http/ mod-http: modules/http/libcurl.la
else
modules/http modules/http/ mod-http:
endif

EXTRA_DIST        +=      \
  modules/http/http-grammar.ym


.PHONY: modules/http/ mod-http
