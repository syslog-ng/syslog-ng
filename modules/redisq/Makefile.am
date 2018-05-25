noinst_LTLIBRARIES += modules/redisq/libsyslog-ng-redis-queue.la
module_LTLIBRARIES += modules/redisq/libredis-queue.la

modules_redisq_libsyslog_ng_redis_queue_la_SOURCES = \
  modules/redisq/redisq-options.h \
  modules/redisq/redisq-options.c \
  modules/redisq/logqueue-redis.c \
  modules/redisq/logqueue-redis.h

modules_redisq_libsyslog_ng_redis_queue_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/redisq
modules_redisq_libsyslog_ng_redis_queue_la_LIBADD	=	\
  $(MODULE_DEPS_LIBS)
modules_redisq_libsyslog_ng_redis_queue_la_DEPENDENCIES	=	\
  $(MODULE_DEPS_LIBS)

modules_redisq_libredis_queue_la_SOURCES = \
  modules/redisq/redisq.c \
  modules/redisq/redisq.h \
  modules/redisq/redisq-grammar.y \
  modules/redisq/redisq-parser.c \
  modules/redisq/redisq-parser.h \
  modules/redisq/redisq-plugin.c

BUILT_SOURCES += \
  modules/redisq/redisq-grammar.y \
  modules/redisq/redisq-grammar.c \
  modules/redisq/redisq-grammar.h

EXTRA_DIST += modules/redisq/redisq-grammar.ym

LIBSYSLOG_NG_REDIS_QUEUE = $(top_builddir)/modules/redisq/libsyslog-ng-redis-queue.la

modules_redisq_libredis_queue_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/redisq -I$(top_builddir)/modules/redisq
modules_redisq_libredis_queue_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBSYSLOG_NG_REDIS_QUEUE)
modules_redisq_libredis_queue_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_redisq_libredis_queue_la_DEPENDENCIES = $(MODULE_DEPS_LIBS) $(LIBSYSLOG_NG_REDIS_QUEUE)

modules/redisq modules/redisq/ mod-redisq: modules/redisq/libredis_queue.la

#include modules/redisq/tests/Makefile.am

.PHONY: modules/redisq/ mod-redisq
