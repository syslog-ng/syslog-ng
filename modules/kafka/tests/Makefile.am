if ENABLE_KAFKA

modules_kafka_tests_TESTS			= \
	modules/kafka/tests/test_kafka_props

check_PROGRAMS					+= ${modules_kafka_tests_TESTS}

modules_kafka_tests_test_kafka_props_SOURCES = 	\
	modules/kafka/tests/test_kafka-props.c
modules_kafka_tests_test_kafka_props_DEPENDENCIES =      \
        $(top_builddir)/modules/kafka/libkafka.la
modules_kafka_tests_test_kafka_props_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/kafka
modules_kafka_tests_test_kafka_props_LDADD	= $(TEST_LDADD)
modules_kafka_tests_test_kafka_props_LDFLAGS	= \
	-dlpreopen $(top_builddir)/modules/kafka/libkafka.la


endif

EXTRA_DIST += \
	modules/kafka/tests/CMakeLists.txt	\
	modules/kafka/tests/sample.properties
