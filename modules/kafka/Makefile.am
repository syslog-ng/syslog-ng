if ENABLE_KAFKA
module_LTLIBRARIES			+=	\
	modules/kafka/libkafka.la

modules_kafka_libkafka_la_CFLAGS	=	\
	$(RDKAFKA_CFLAGS)			\
	-I$(top_srcdir)/modules/kafka		\
	-I$(top_builddir)/modules/kafka
modules_kafka_libkafka_la_SOURCES	=	\
	modules/kafka/kafka-grammar.y 		\
	modules/kafka/kafka.c			        \
	modules/kafka/kafka.h						  \
	modules/kafka/kafka-parser.h      \
	modules/kafka/kafka-parser.c			\
  modules/kafka/crc32.c

modules_kafka_libkafka_la_LIBADD	=	\
	-lrdkafka $(RDKAFKA_LIBS) $(MODULE_DEPS_LIBS)
modules_kafka_libkafka_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_kafka_libkafka_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/kafka modules/kafka/ mod-kafka: \
	modules/kafka/libkafka.la
else
modules/kafka modules/kafka/ mod-kafka:
endif

BUILT_SOURCES				+=	\
	modules/kafka/kafka-grammar.y		\
	modules/kafka/kafka-grammar.c		\
	modules/kafka/kafka-grammar.h
EXTRA_DIST				+=	\
	modules/kafka/kafka-grammar.ym

.PHONY: modules/kafka/ mod-kafka
