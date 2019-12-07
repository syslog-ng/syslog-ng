if ENABLE_HTTP

module_LTLIBRARIES      += modules/http/libhttp.la
modules_http_libhttp_la_SOURCES = \
  modules/http/response-handler.h   \
  modules/http/response-handler.c   \
  modules/http/http.h 		    \
  modules/http/http.c               \
  modules/http/http-worker.c	    \
  modules/http/http-worker.h	    \
  modules/http/http-loadbalancer.c  \
  modules/http/http-loadbalancer.h  \
  modules/http/http-grammar.y       \
  modules/http/http-parser.c        \
  modules/http/http-parser.h        \
  modules/http/http-plugin.c        \
  modules/http/http-signals.h

modules_http_libhttp_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  $(LIBCURL_CFLAGS)          \
  -I$(top_srcdir)/modules/http        \
  -I$(top_builddir)/modules/http

modules_http_libhttp_la_LIBADD  = $(MODULE_DEPS_LIBS) $(LIBCURL_LIBS)

modules_http_libhttp_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_http_libhttp_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/http modules/http/ mod-http: modules/http/libhttp.la
else
modules/http modules/http/ mod-http:
endif

BUILT_SOURCES       +=      \
  modules/http/http-grammar.y       \
  modules/http/http-grammar.c       \
  modules/http/http-grammar.h

EXTRA_DIST        +=      \
  modules/http/http-grammar.ym	\
  modules/http/CMakeLists.txt	\
  modules/http/README.md

.PHONY: modules/http/ mod-http

include modules/http/tests/Makefile.am
