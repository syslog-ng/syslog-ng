gRPC based destination drivers: Added gRPC request related metrics.

Affected drivers:
  * `opentelemetry()`
  * `syslog-ng-otlp()`
  * `bigquery()`
  * `loki()`

Example metrics:
```
syslogng_output_grpc_requests_total{driver="syslog-ng-otlp",url="localhost:12345",response_code="ok"} 49
syslogng_output_grpc_requests_total{driver="syslog-ng-otlp",url="localhost:12345",response_code="unavailable"} 11
```
