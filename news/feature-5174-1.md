`opentelemetry()`, `axosyslog-otlp()`, `syslog-ng-otlp()`: Added `keep-alive()` options.

Keepalive can be configured with the `time()`, `timeout()`
and `max-pings-without-data()` options of the `keep-alive()` block.

```
opentelemetry(
    ...
    keep-alive(time(20000) timeout(10000) max-pings-without-data(0))
);
```
