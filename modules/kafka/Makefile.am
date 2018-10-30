noinst_LTLIBRARIES += modules/kafka/libkafka.la

modules_kafka_libkafka_la_SOURCES = \
  modules/kafka/kafka-grammar.y \
  modules/kafka/kafka-parser.c \
  modules/kafka/kafka-parser.h \
  modules/kafka/kafka-source.c \
  modules/kafka/kafka-source.h \
  modules/kafka/kafka-plugin.c

modules_kafka_libkafka_la_CPPFLAGS  = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/kafka \
  -I$(top_builddir)/modules/kafka

modules_kafka_libkafka_la_LIBADD  = \
  $(MODULE_DEPS_LIBS)

modules_kafka_libkafka_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_kafka_libkafka_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS)

modules/kafka modules/kafka/ mod-kafka: \
	modules/kafka/libkafka.la

BUILT_SOURCES += \
  modules/kafka/kafka-grammar.y \
  modules/kafka/kafka-grammar.c \
  modules/kafka/kafka-grammar.h

EXTRA_DIST += \
  modules/kafka/kafka-grammar.ym \
  modules/kafka/CMakeLists.txt

.PHONY: modules/kafka/ mod-kafka

