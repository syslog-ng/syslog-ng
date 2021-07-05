kafka-c: fixed a hang during shutdown/reload, when multiple workers is used (workers() option is set to 2 or higher) and the librdkafka internal queue is filled.
(error message was `kafka: failed to publish message; topic='test-topic', error='Local: Queue full'`)
