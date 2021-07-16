add_unit_test(CRITERION LIBTEST TARGET test_kafka-props DEPENDS kafka)
add_unit_test(CRITERION LIBTEST TARGET test_kafka_topic DEPENDS kafka rdkafka)
add_unit_test(CRITERION LIBTEST TARGET test_kafka_config DEPENDS kafka rdkafka)
