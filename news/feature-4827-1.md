`opentelemetry()`, `syslog-ng-otlp()` source: Added `log-fetch-limit()` option.

This option can be used to fine tune the performance. To minimize locking while
moving messages between source and destination side queues, syslog-ng can move
messages in batches. The `log-fetch-limit()` option sets the maximal size of
the batch moved by a worker. By default it is equal to `log-iw-size() / workers()`.
