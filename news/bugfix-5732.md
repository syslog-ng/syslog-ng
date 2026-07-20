`kafka`: fix several source driver bugs

Fix a crash when the configured `topic()` is empty, propagate persist-state init failures, plug leaks of the
`rd_kafka_conf_t` and topic-partition list, and correct `logging()` option value matching.
