destinations: Added "syslogng_output_event_retries_total" counter.

This counter is available for the following destination drivers:
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

Example metrics:
```
syslogng_output_event_retries_total{driver="http",url="http://localhost:8888/${path}",id="#anon-destination0#0"} 5
```
