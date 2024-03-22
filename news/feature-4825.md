destinations: Added a new exponential backoff reopen strategy

Affected drivers are:
 * `amqp()`
 * `bigquery()`
 * `http()` and all http based drivers
 * `java()`
 * `kafka()`
 * `loki()`
 * `mongodb()`
 * `mqtt()`
 * `opentelemetry()`
 * `python()` and all python based drivers
 * `redis()`
 * `riemann()`
 * `smtp()`
 * `snmp()`
 * `sql()`
 * `stomp()`
 * `syslog-ng-otlp()`

With syslog-ng 4.7 the default reopen strategy changed to this new
exponential backoff logic.

You can use the `reopen()` block to configure its parameters.

```
reopen(
  initial-seconds(1.5)
  maximum-seconds(10)
  multiplier(1.1)
)
```

Default values are `initial-seconds(0.1)`, `maximum-seconds(1)` and `multiplier(2)`.

If you wish to use the old constant `time-reopen()` reopen strategy,
make sure to use an older `@version` in your config, e.g. `@version: 4.6`.

We are planning to eventually introduce this reopen strategy to every driver
to replace `time-reopen()`.
