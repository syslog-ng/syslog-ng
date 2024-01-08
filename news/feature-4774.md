`opentelemetry()`, `syslog-ng-otlp()`: Added `workers()` option on source side.

This feature enables processing the OTLP messages on multiple threads,
which can greatly improve the performance.
