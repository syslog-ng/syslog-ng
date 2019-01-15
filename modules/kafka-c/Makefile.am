if ENABLE_KAFKA
module_LTLIBRARIES			+=	\
	modules/kafka-c/libkafka-c.la

modules_kafka_c_libkafka_c_la_CFLAGS	=	\
	$(RDKAFKA_CFLAGS)			\
	$(INCUBATOR_CFLAGS)			\
	-I$(top_srcdir)/modules/kafka-c		\
	-I$(top_builddir)/modules/kafka-c
modules_kafka_c_libkafka_c_la_SOURCES	=	\
	modules/kafka-c/kafka-grammar.y 		\
	modules/kafka-c/kafka.c			        \
	modules/kafka-c/kafka.h						  \
	modules/kafka-c/kafka-parser.h      \
	modules/kafka-c/kafka-parser.c

modules_kafka_c_libkafka_c_la_LIBADD	=	\
	$(RDKAFKA_LIBS) $(INCUBATOR_LIBS)

modules_kafka_c_libkafka_c_la_LDFLAGS	=	\
	-avoid-version -module -no-undefined

modules/kafka-c modules/kafka-c/ mod-kafka-c: \
	modules/kafka-c/libkafka-c.la
else
modules/kafka-c modules/kafka-c/ mod-kafka-c:
endif

BUILT_SOURCES				+=	\
	modules/kafka-c/kafka-grammar.y		\
	modules/kafka-c/kafka-grammar.c		\
	modules/kafka-c/kafka-grammar.h
EXTRA_DIST				+=	\
	modules/kafka-c/kafka-grammar.ym

.PHONY: modules/kafka-c/ mod-kafka-c
