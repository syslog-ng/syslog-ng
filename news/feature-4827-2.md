`opentelemetry()`, `syslog-ng-otlp()` source: Added `concurrent-requests()` option.

This option configures the maximal number of in-flight gRPC requests per worker.
Setting this value to the range of 10s or 100s is recommended when there are a
high number of clients sending simultaneously.

Ideally, `workers() * concurrent-requests()` should be greater or equal to
the number of clients, but this can increase the memory usage.
